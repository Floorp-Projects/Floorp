/* jshint moz:true, browser:true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ['PeerConnectionIdp'];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'IdpSandbox',
  'resource://gre/modules/media/IdpSandbox.jsm');

/**
 * Creates an IdP helper.
 *
 * @param win (object) the window we are working for
 * @param timeout (int) the timeout in milliseconds
 */
function PeerConnectionIdp(win, timeout) {
  this._win = win;
  this._timeout = timeout || 5000;

  this.provider = null;
  this._resetAssertion();
}

(function() {
  PeerConnectionIdp._mLinePattern = new RegExp('^m=', 'm');
  // attributes are funny, the 'a' is case sensitive, the name isn't
  let pattern = '^a=[iI][dD][eE][nN][tT][iI][tT][yY]:(\\S+)';
  PeerConnectionIdp._identityPattern = new RegExp(pattern, 'm');
  pattern = '^a=[fF][iI][nN][gG][eE][rR][pP][rR][iI][nN][tT]:(\\S+) (\\S+)';
  PeerConnectionIdp._fingerprintPattern = new RegExp(pattern, 'm');
})();

PeerConnectionIdp.prototype = {
  get enabled() {
    return !!this._idp;
  },

  _resetAssertion: function() {
    this.assertion = null;
    this.idpLoginUrl = null;
  },

  setIdentityProvider: function(provider, protocol, username) {
    this._resetAssertion();
    this.provider = provider;
    this.protocol = protocol || 'default';
    this.username = username;
    if (this._idp) {
      if (this._idp.isSame(provider, protocol)) {
        return; // noop
      }
      this._idp.stop();
    }
    this._idp = new IdpSandbox(provider, protocol);
  },

  // start the IdP and do some error fixup
  start: function() {
    return this._idp.start()
      .catch(e => {
        throw new this._win.DOMException(e.message, 'IdpError');
      });
  },

  close: function() {
    this._resetAssertion();
    this.provider = null;
    this.protocol = null;
    if (this._idp) {
      this._idp.stop();
      this._idp = null;
    }
  },

  _getFingerprintsFromSdp: function(sdp) {
    let fingerprints = {};
    let m = sdp.match(PeerConnectionIdp._fingerprintPattern);
    while (m) {
      fingerprints[m[0]] = { algorithm: m[1], digest: m[2] };
      sdp = sdp.substring(m.index + m[0].length);
      m = sdp.match(PeerConnectionIdp._fingerprintPattern);
    }

    return Object.keys(fingerprints).map(k => fingerprints[k]);
  },

  _isValidAssertion: function(assertion) {
    return assertion && assertion.idp &&
      typeof assertion.idp.domain === 'string' &&
      (!assertion.idp.protocol ||
       typeof assertion.idp.protocol === 'string') &&
      typeof assertion.assertion === 'string';
  },

  _getIdentityFromSdp: function(sdp) {
    // a=identity is session level
    let idMatch;
    let mLineMatch = sdp.match(PeerConnectionIdp._mLinePattern);
    if (mLineMatch) {
      let sessionLevel = sdp.substring(0, mLineMatch.index);
      idMatch = sessionLevel.match(PeerConnectionIdp._identityPattern);
    }
    if (!idMatch) {
      return; // undefined === no identity
    }

    let assertion;
    try {
      assertion = JSON.parse(atob(idMatch[1]));
    } catch (e) {
      throw new this._win.DOMException('invalid identity assertion: ' + e,
                                       'InvalidSessionDescriptionError');
    }
    if (!this._isValidAssertion(assertion)) {
      throw new this._win.DOMException('assertion missing idp/idp.domain/assertion',
                                       'InvalidSessionDescriptionError');
    }
    return assertion;
  },

  /**
   * Verifies the a=identity line the given SDP contains, if any.
   * If the verification succeeds callback is called with the message from the
   * IdP proxy as parameter, else (verification failed OR no a=identity line in
   * SDP at all) null is passed to callback.
   *
   * Note that this only verifies that the SDP is coherent.  We still rely on
   * the fact that the RTCPeerConnection won't connect to a peer if the
   * fingerprint of the certificate they offer doesn't appear in the SDP.
   */
  verifyIdentityFromSDP: function(sdp, origin) {
    let identity = this._getIdentityFromSdp(sdp);
    let fingerprints = this._getFingerprintsFromSdp(sdp);
    if (!identity || fingerprints.length <= 0) {
      return this._win.Promise.resolve(); // undefined result = no identity
    }

    this.setIdentityProvider(identity.idp.domain, identity.idp.protocol);
    return this._verifyIdentity(identity.assertion, fingerprints, origin);
  },

  /**
   * Checks that the name in the identity provided by the IdP is OK.
   *
   * @param name (string) the name to validate
   * @throws if the name isn't valid
   */
  _validateName: function(name) {
    let error = msg => {
        throw new this._win.DOMException('assertion name error: ' + msg,
                                         'IdpError');
    };

    if (typeof name !== 'string') {
      error('name not a string');
    }
    let atIdx = name.indexOf('@');
    if (atIdx <= 0) {
      error('missing authority in name from IdP');
    }

    // no third party assertions... for now
    let tail = name.substring(atIdx + 1);

    // strip the port number, if present
    let provider = this.provider;
    let providerPortIdx = provider.indexOf(':');
    if (providerPortIdx > 0) {
      provider = provider.substring(0, providerPortIdx);
    }
    let idnService = Components.classes['@mozilla.org/network/idn-service;1']
        .getService(Components.interfaces.nsIIDNService);
    if (idnService.convertUTF8toACE(tail) !==
        idnService.convertUTF8toACE(provider)) {
      error('name "' + identity.name +
            '" doesn\'t match IdP: "' + this.provider + '"');
    }
  },

  /**
   * Check the validation response.  We are very defensive here when handling
   * the message from the IdP proxy.  That way, broken IdPs aren't likely to
   * cause catastrophic damage.
   */
  _checkValidation: function(validation, sdpFingerprints) {
    let error = msg => {
      throw new this._win.DOMException('IdP validation error: ' + msg,
                                       'IdpError');
    };

    if (!this.provider) {
      error('IdP closed');
    }

    if (typeof validation !== 'object' ||
        typeof validation.contents !== 'string' ||
        typeof validation.identity !== 'string') {
      error('no payload in validation response');
    }

    let fingerprints;
    try {
      fingerprints = JSON.parse(validation.contents).fingerprint;
    } catch (e) {
      error('invalid JSON');
    }

    let isFingerprint = f =>
        (typeof f.digest === 'string') &&
        (typeof f.algorithm === 'string');
    if (!Array.isArray(fingerprints) || !fingerprints.every(isFingerprint)) {
      error('fingerprints must be an array of objects' +
            ' with digest and algorithm attributes');
    }

    // everything in `innerSet` is found in `outerSet`
    let isSubsetOf = (outerSet, innerSet, comparator) => {
      return innerSet.every(i => {
        return outerSet.some(o => comparator(i, o));
      });
    };
    let compareFingerprints = (a, b) => {
      return (a.digest === b.digest) && (a.algorithm === b.algorithm);
    };
    if (!isSubsetOf(fingerprints, sdpFingerprints, compareFingerprints)) {
      error('the fingerprints must be covered by the assertion');
    }
    this._validateName(validation.identity);
    return validation;
  },

  /**
   * Asks the IdP proxy to verify an identity assertion.
   */
  _verifyIdentity: function(assertion, fingerprints, origin) {
    let p = this.start()
        .then(idp => this._wrapCrossCompartmentPromise(
          idp.validateAssertion(assertion, origin)))
        .then(validation => this._checkValidation(validation, fingerprints));

    return this._applyTimeout(p);
  },

  /**
   * Enriches the given SDP with an `a=identity` line.  getIdentityAssertion()
   * must have already run successfully, otherwise this does nothing to the sdp.
   */
  addIdentityAttribute: function(sdp) {
    if (!this.assertion) {
      return sdp;
    }

    // yes, we assume that this matches; if it doesn't something is *wrong*
    let match = sdp.match(PeerConnectionIdp._mLinePattern);
    return sdp.substring(0, match.index) +
      'a=identity:' + this.assertion + '\r\n' +
      sdp.substring(match.index);
  },

  /**
   * Asks the IdP proxy for an identity assertion.  Don't call this unless you
   * have checked .enabled, or you really like exceptions.  Also, don't call
   * this when another call is still running, because it's not certain which
   * call will finish first and the final state will be similarly uncertain.
   */
  getIdentityAssertion: function(fingerprint, origin) {
    if (!this.enabled) {
      throw new this._win.DOMException(
        'no IdP set, call setIdentityProvider() to set one', 'InvalidStateError');
    }

    let [algorithm, digest] = fingerprint.split(' ', 2);
    let content = {
      fingerprint: [{
        algorithm: algorithm,
        digest: digest
      }]
    };

    this._resetAssertion();
    let p = this.start()
        .then(idp => this._wrapCrossCompartmentPromise(
          idp.generateAssertion(JSON.stringify(content),
                                origin, this.username)))
        .then(assertion => {
          if (!this._isValidAssertion(assertion)) {
            throw new this._win.DOMException('IdP generated invalid assertion',
                                             'IdpError');
          }
          // save the base64+JSON assertion, since that is all that is used
          this.assertion = btoa(JSON.stringify(assertion));
          return this.assertion;
        });

    return this._applyTimeout(p);
  },

  /**
   * Promises generated by the sandbox need to be very carefully treated so that
   * they can chain into promises in the `this._win` compartment.  Results need
   * to be cloned across; errors need to be converted.
   */
  _wrapCrossCompartmentPromise: function(sandboxPromise) {
    return new this._win.Promise((resolve, reject) => {
      sandboxPromise.then(
        result => resolve(Cu.cloneInto(result, this._win)),
        e => {
          let message = '' + (e.message || JSON.stringify(e) || 'IdP error');
          if (e.name === 'IdpLoginError') {
            if (typeof e.loginUrl === 'string') {
              this.idpLoginUrl = e.loginUrl;
            }
            reject(new this._win.DOMException(message, 'IdpLoginError'));
          } else {
            reject(new this._win.DOMException(message, 'IdpError'));
          }
        });
    });
  },

  /**
   * Wraps a promise, adding a timeout guard on it so that it can't take longer
   * than the specified time.  Returns a promise that rejects if the timeout
   * elapses before `p` resolves.
   */
  _applyTimeout: function(p) {
    let timeout = new this._win.Promise(
      r => this._win.setTimeout(r, this._timeout))
        .then(() => {
          throw new this._win.DOMException('IdP timed out', 'IdpError');
        });
    return this._win.Promise.race([ timeout, p ]);
  }
};

this.PeerConnectionIdp = PeerConnectionIdp;
