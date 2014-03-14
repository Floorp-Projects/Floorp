/* jshint moz:true, browser:true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["PeerConnectionIdp"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "IdpProxy",
  "resource://gre/modules/media/IdpProxy.jsm");

/**
 * Creates an IdP helper.
 *
 * @param window (object) the window object to use for miscellaneous goodies
 * @param timeout (int) the timeout in milliseconds
 * @param warningFunc (function) somewhere to dump warning messages
 */
function PeerConnectionIdp(window, timeout, warningFunc) {
  this._win = window;
  this._timeout = timeout || 5000;
  this._warning = warningFunc;

  this.assertion = null;
  this.provider = null;
}

(function() {
  PeerConnectionIdp._mLinePattern = new RegExp("^m=", "m");
  // attributes are funny, the 'a' is case sensitive, the name isn't
  let pattern = "^a=[iI][dD][eE][nN][tT][iI][tT][yY]:(\\S+)";
  PeerConnectionIdp._identityPattern = new RegExp(pattern, "m");
  pattern = "^a=[fF][iI][nN][gG][eE][rR][pP][rR][iI][nN][tT]:(\\S+) (\\S+)";
  PeerConnectionIdp._fingerprintPattern = new RegExp(pattern, "m");
})();

PeerConnectionIdp.prototype = {
  setIdentityProvider: function(provider, protocol, username) {
    this.provider = provider;
    this.username = username;
    if (this._idpchannel) {
      if (this._idpchannel.isSame(provider, protocol)) {
        return;
      }
      this._idpchannel.close();
    }
    this._idpchannel = new IdpProxy(provider, protocol);
  },

  close: function() {
    this.assertion = null;
    this.provider = null;
    if (this._idpchannel) {
      this._idpchannel.close();
      this._idpchannel = null;
    }
  },

  _getFingerprintFromSdp: function(sdp) {
    let sections = sdp.split(PeerConnectionIdp._mLinePattern);
    let attributes = sections.map(function(sect) {
      let m = sect.match(PeerConnectionIdp._fingerprintPattern);
      if (m) {
        let remainder = sect.substring(m.index + m[0].length);
        if (!remainder.match(PeerConnectionIdp._fingerprintPattern)) {
          return { algorithm: m[1], digest: m[2] };
        }
        this._warning("RTC identity: two fingerprint values in same media " +
            "section are not supported", null, 0);
        // we have to return non-falsy here so that a media section doesn't
        // accidentally fall back to the session-level stuff (which is bad)
        return "error";
      }
      // return undefined unless there is exactly one match
    }, this);

    let sessionLevel = attributes.shift();
    attributes = attributes.map(function(sectionLevel) {
      return sectionLevel || sessionLevel;
    });

    let first = attributes.shift();
    function sameAsFirst(attr) {
      return typeof attr === "object" &&
      first.algorithm === attr.algorithm &&
      first.digest === attr.digest;
    }

    if (typeof first === "object" && attributes.every(sameAsFirst)) {
      return first;
    }
    // undefined!
  },

  _getIdentityFromSdp: function(sdp) {
    // we only pull from the session level right now
    // TODO allow for per-m=-section identity
    let mLineMatch = sdp.match(PeerConnectionIdp._mLinePattern);
    let sessionLevel = sdp.substring(0, mLineMatch.index);
    let idMatch = sessionLevel.match(PeerConnectionIdp._identityPattern);
    if (idMatch) {
      let assertion = {};
      try {
        assertion = JSON.parse(atob(idMatch[1]));
      } catch (e) {
        this._warning("RTC identity: invalid identity assertion: " + e, null, 0);
      } // for JSON.parse
      if (typeof assertion.idp === "object" &&
          typeof assertion.idp.domain === "string" &&
          typeof assertion.assertion === "string") {
        return assertion;
      }
      this._warning("RTC identity: assertion missing idp/idp.domain/assertion",
                    null, 0);
    }
    // undefined!
  },

  /**
   * Queues a task to verify the a=identity line the given SDP contains, if any.
   * If the verification succeeds callback is called with the message from the
   * IdP proxy as parameter, else (verification failed OR no a=identity line in
   * SDP at all) null is passed to callback.
   */
  verifyIdentityFromSDP: function(sdp, callback) {
    let identity = this._getIdentityFromSdp(sdp);
    let fingerprint = this._getFingerprintFromSdp(sdp);
    // it's safe to use the fingerprint from the SDP here,
    // only because we ensure that there is only one
    if (!fingerprint || !identity) {
      callback(null);
      return;
    }
    this.setIdentityProvider(identity.idp.domain, identity.idp.protocol);

    this._verifyIdentity(identity.assertion, fingerprint, callback);
  },

  /**
   * Checks that the name in the identity provided by the IdP is OK.
   *
   * @param name (string) the name to validate
   * @returns (string) an error message, iff the name isn't good
   */
  _validateName: function(name) {
    if (typeof name !== "string") {
      return "name not a string";
    }
    let atIdx = name.indexOf("@");
    if (atIdx > 0) {
      // no third party assertions... for now
      let tail = name.substring(atIdx + 1);

      // strip the port number, if present
      let provider = this.provider;
      let providerPortIdx = provider.indexOf(":");
      if (providerPortIdx > 0) {
        provider = provider.substring(0, providerPortIdx);
      }
      var idnService = Components.classes["@mozilla.org/network/idn-service;1"].
        getService(Components.interfaces.nsIIDNService);
      if (idnService.convertUTF8toACE(tail) !==
          idnService.convertUTF8toACE(provider)) {
        return "name '" + identity.name +
            "' doesn't match IdP: '" + this.provider + "'";
      }
      return null;
    }
    return "missing authority in name from IdP";
  },

  // we are very defensive here when handling the message from the IdP
  // proxy so that broken IdPs can only do as little harm as possible.
  _checkVerifyResponse: function(
      message, fingerprint) {
    let warn = function(message) {
      this._warning("RTC identity: VERIFY error: " + message, null, 0);
    }.bind(this);

    try {
      let contents = JSON.parse(message.contents);
      if (typeof contents.fingerprint !== "object" ||
          typeof message.identity !== "object") {
        warn("fingerprint or identity not objects");
      } else if (contents.fingerprint.digest !== fingerprint.digest ||
          contents.fingerprint.algorithm !== fingerprint.algorithm) {
        warn("fingerprint does not match");
      } else {
        let error = this._validateName(message.identity.name);
        if (error) {
          warn(error);
        } else {
          return true;
        }
      }
    } catch(e) {
      warn("invalid JSON in content");
    }
    return false;
  },

  /**
   * Asks the IdP proxy to verify an identity.
   */
  _verifyIdentity: function(
      assertion, fingerprint, callback) {
    function onVerification(message) {
      if (!message) {
        this._warning("RTC identity: verification failure", null, 0);
        callback(null);
        return;
      }
      if (this._checkVerifyResponse(message, fingerprint)) {
        callback(message);
      } else {
        callback(null);
      }
    }

    let request = {
      type: "VERIFY",
      message: assertion
    };
    this._sendToIdp(request, onVerification.bind(this));
  },

  /**
   * Asks the IdP proxy for an identity assertion and, on success, enriches the
   * given SDP with an a=identity line and calls callback with the new SDP as
   * parameter. If no IdP is configured the original SDP (without a=identity
   * line) is passed to the callback.
   */
  appendIdentityToSDP: function(sdp, fingerprint, callback) {
    if (!this._idpchannel) {
      callback(sdp);
      return;
    }

    if (this.assertion) {
      callback(this.wrapSdp(sdp));
      return;
    }

    function onAssertion(assertion) {
      callback(this.wrapSdp(sdp), assertion);
    }

    this._getIdentityAssertion(fingerprint, onAssertion.bind(this));
  },

  /**
   * Inserts an identity assertion into the given SDP.
   */
  wrapSdp: function(sdp) {
    if (!this.assertion) {
      return sdp;
    }

    // yes, we assume that this matches; if it doesn't something is *wrong*
    let match = sdp.match(PeerConnectionIdp._mLinePattern);
    return sdp.substring(0, match.index) +
      "a=identity:" + this.assertion + "\r\n" +
      sdp.substring(match.index);
  },

  getIdentityAssertion: function(fingerprint, callback) {
    if (!this._idpchannel) {
      throw new Error("IdP not set");
    }

    this._getIdentityAssertion(fingerprint, callback);
  },

  _getIdentityAssertion: function(fingerprint, callback) {
    let [algorithm, digest] = fingerprint.split(" ");
    let message = {
      fingerprint: {
        algorithm: algorithm,
        digest: digest
      }
    };
    let request = {
      type: "SIGN",
      message: JSON.stringify(message),
      username: this.username
    };

    // catch the assertion, clean it up, warn if absent
    function trapAssertion(assertion) {
      if (!assertion) {
        this._warning("RTC identity: assertion generation failure", null, 0);
        this.assertion = null;
      } else {
        this.assertion = btoa(JSON.stringify(assertion));
      }
      callback(this.assertion);
    }

    this._sendToIdp(request, trapAssertion.bind(this));
  },

  /**
   * Packages a message and sends it to the IdP.
   */
  _sendToIdp: function(request, callback) {
    // this is not secure
    // but there are no good alternatives until bug 968335 lands
    // when that happens, change this to use the new mechanism
    request.origin = this._win.document.nodePrincipal.origin;

    this._idpchannel.send(request, this._wrapCallback(callback));
  },

  /**
   * Wraps a callback, adding a timeout and ensuring that the callback doesn't
   * receive any message other than one where the IdP generated a "SUCCESS"
   * response.
   */
  _wrapCallback: function(callback) {
    let timeout = this._win.setTimeout(function() {
      this._warning("RTC identity: IdP timeout for " + this._idpchannel + " " +
           (this._idpchannel.ready ? "[ready]" : "[not ready]"), null, 0);
      timeout = null;
      callback(null);
    }.bind(this), this._timeout);

    return function(message) {
      if (!timeout) {
        return;
      }
      this._win.clearTimeout(timeout);
      timeout = null;
      var content = null;
      if (message.type === "SUCCESS") {
        content = message.message;
      } else {
        this._warning("RTC Identity: received response of type '" +
            message.type + "' from IdP: " + message.message, null, 0);
      }
      callback(content);
    }.bind(this);
  }
};

this.PeerConnectionIdp = PeerConnectionIdp;
