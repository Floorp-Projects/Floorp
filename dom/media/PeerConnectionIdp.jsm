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

function TimerResolver(resolve) {
  this.notify = resolve;
}
TimerResolver.prototype = {
  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback])
}
function delay(t) {
  return new Promise(resolve => {
    let timer = Cc['@mozilla.org/timer;1'].getService(Ci.nsITimer);
    timer.initWithCallback(new TimerResolver(resolve), t, 0); // One shot
  });
}

/**
 * Creates an IdP helper.
 *
 * @param timeout (int) the timeout in milliseconds
 * @param warningFunc (function) somewhere to dump warning messages
 * @param dispatchErrorFunc (function(string, dict)) somewhere to dump errors
 */
function PeerConnectionIdp(timeout, warningFunc, dispatchErrorFunc) {
  this._timeout = timeout || 5000;
  this._warning = warningFunc;
  this._dispatchError = dispatchErrorFunc;

  this.assertion = null;
  this.provider = null;
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

  setIdentityProvider: function(provider, protocol, username) {
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

  close: function() {
    this.assertion = null;
    this.provider = null;
    this.protocol = null;
    if (this._idp) {
      this._idp.stop();
      this._idp = null;
    }
  },

  /**
   * Generate an error event of the identified type;
   * and put a little more precise information in the console.
   *
   * A little note on error handling in this class: this class reports errors
   * exclusively through the event handlers that are passed to it
   * (this._dispatchError, specifically).  That means that all the functions
   * return resolved promises; promises are never rejected.  This probably isn't
   * the best design, but the refactor can wait.
   */
  reportError: function(type, message, extra) {
    let args = {
      idp: this.provider,
      protocol: this.protocol
    };
    if (extra) {
      Object.keys(extra).forEach(function(k) {
        args[k] = extra[k];
      });
    }
    this._warning('RTC identity: ' + message, null, 0);
    this._dispatchError('idp' + type + 'error', args);
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
      let idMatch = sessionLevel.match(PeerConnectionIdp._identityPattern);
    }
    if (!idMatch) {
      return; // undefined === no identity
    }

    let assertion;
    try {
      assertion = JSON.parse(atob(idMatch[1]));
    } catch (e) {
      this.reportError('validation',
                       'invalid identity assertion: ' + e);
    }
    if (!this._isValidAssertion(assertion)) {
      this.reportError('validation', 'assertion missing' +
                       ' idp/idp.domain/assertion');
    }
    return assertion;
  },

  /**
   * Verifies the a=identity line the given SDP contains, if any.
   * If the verification succeeds callback is called with the message from the
   * IdP proxy as parameter, else (verification failed OR no a=identity line in
   * SDP at all) null is passed to callback.
   *
   * Note that this only verifies that the SDP is coherent.  This relies on the
   * invariant that the RTCPeerConnection won't connect to a peer if the
   * fingerprint of the certificate they offer doesn't appear in the SDP.
   */
  verifyIdentityFromSDP: function(sdp, origin) {
    let identity = this._getIdentityFromSdp(sdp);
    let fingerprints = this._getFingerprintsFromSdp(sdp);
    if (!identity || fingerprints.length <= 0) {
      return Promise.resolve();
    }

    this.setIdentityProvider(identity.idp.domain, identity.idp.protocol);
    return this._verifyIdentity(identity.assertion, fingerprints, origin);
  },

  /**
   * Checks that the name in the identity provided by the IdP is OK.
   *
   * @param error (function) an error function to call
   * @param name (string) the name to validate
   * @throws if the name isn't valid
   */
  _validateName: function(error, name) {
    if (typeof name !== 'string') {
      return error('name not a string');
    }
    let atIdx = name.indexOf('@');
    if (atIdx <= 0) {
      return error('missing authority in name from IdP');
    }

    // no third party assertions... for now
    let tail = name.substring(atIdx + 1);

    // strip the port number, if present
    let provider = this.provider;
    let providerPortIdx = provider.indexOf(':');
    if (providerPortIdx > 0) {
      provider = provider.substring(0, providerPortIdx);
    }
    let idnService = Components.classes['@mozilla.org/network/idn-service;1'].
      getService(Components.interfaces.nsIIDNService);
    if (idnService.convertUTF8toACE(tail) !==
        idnService.convertUTF8toACE(provider)) {
      return error('name "' + identity.name +
            '" doesn\'t match IdP: "' + this.provider + '"');
    }
    return true;
  },

  /**
   * Check the validation response.  We are very defensive here when handling
   * the message from the IdP proxy.  That way, broken IdPs aren't likely to
   * cause catastrophic damage.
   */
  _isValidVerificationResponse: function(validation, sdpFingerprints) {
    let error = msg => {
      this.reportError('validation', 'assertion validation failure: ' + msg);
      return false;
    };

    if (typeof validation !== 'object' ||
        typeof validation.contents !== 'string' ||
        typeof validation.identity !== 'string') {
      return error('no payload in validation response');
    }

    let fingerprints;
    try {
      fingerprints = JSON.parse(validation.contents).fingerprint;
    } catch (e) {
      return error('idp returned invalid JSON');
    }

    let isFingerprint = f =>
        (typeof f.digest === 'string') &&
        (typeof f.algorithm === 'string');
    if (!Array.isArray(fingerprints) || !fingerprints.every(isFingerprint)) {
      return error('fingerprints must be an array of objects' +
                   ' with digest and algorithm attributes');
    }

    let isSubsetOf = (outerSet, innerSet, comparator) => {
      return innerSet.every(i => {
        return outerSet.some(o => comparator(i, o));
      });
    };
    let compareFingerprints = (a, b) => {
      return (a.digest === b.digest) && (a.algorithm === b.algorithm);
    };
    if (!isSubsetOf(fingerprints, sdpFingerprints, compareFingerprints)) {
      return error('the fingerprints in SDP aren\'t covered by the assertion');
    }
    return this._validateName(error, validation.identity);
  },

  /**
   * Asks the IdP proxy to verify an identity assertion.
   */
  _verifyIdentity: function(assertion, fingerprints, origin) {
    let validationPromise = this._idp.start()
        .then(idp => idp.validateAssertion(assertion, origin));

    return this._safetyNet('validation', validationPromise)
      .then(validation => {
        if (validation &&
            this._isValidVerificationResponse(validation, fingerprints)) {
          return validation;
        }
      });
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
   * have checked .enabled, or you really like exceptions.
   */
  getIdentityAssertion: function(fingerprint) {
    if (!this.enabled) {
      this.reportError('assertion', 'no IdP set,' +
                       ' call setIdentityProvider() to set one');
      return Promise.resolve();
    }

    let [algorithm, digest] = fingerprint.split(' ', 2);
    let content = {
      fingerprint: [{
        algorithm: algorithm,
        digest: digest
      }]
    };
    let origin = Cu.getWebIDLCallerPrincipal().origin;

    let assertionPromise = this._idp.start()
        .then(idp => idp.generateAssertion(JSON.stringify(content),
                                           origin, this.username));

    return this._safetyNet('assertion', assertionPromise)
      .then(assertion => {
        if (this._isValidAssertion(assertion)) {
          // save the base64+JSON assertion, since that is all that is used
          this.assertion = btoa(JSON.stringify(assertion));
        } else {
          if (assertion) {
            // only report an error for an invalid assertion
            // other paths generate more specific error reports
            this.reportError('assertion', 'invalid assertion generated');
          }
          this.assertion = null;
        }
        return this.assertion;
      });
  },

  /**
   * Wraps a promise, adding a timeout guard on it so that it can't take longer
   * than the specified time.  Returns a promise that always resolves; if there
   * is a problem the resolved value is undefined.
   */
  _safetyNet: function(type, p) {
    let done = false; // ... all because Promises don't expose state
    let timeoutPromise = delay(this._timeout)
        .then(() => {
          if (!done) {
            this.reportError(type, 'IdP timed out');
          }
        });
    let realPromise = p
        .catch(e => this.reportError(type, 'error reported by IdP: ' + e.message))
        .then(result => {
          done = true;
          return result;
        });
    // If timeoutPromise completes first, the returned value will be undefined,
    // just like when there is an error.
    return Promise.race([realPromise, timeoutPromise]);
  }
};

this.PeerConnectionIdp = PeerConnectionIdp;
