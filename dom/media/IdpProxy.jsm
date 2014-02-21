/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["IdpProxy"];

const {
  classes: Cc,
  interfaces: Ci,
  utils: Cu,
  results: Cr
} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Sandbox",
                                  "resource://gre/modules/identity/Sandbox.jsm");

/**
 * An invisible iframe for hosting the idp shim.
 *
 * There is no visible UX here, as we assume the user has already
 * logged in elsewhere (on a different screen in the web site hosting
 * the RTC functions).
 */
function IdpChannel(uri, messageCallback) {
  this.sandbox = null;
  this.messagechannel = null;
  this.source = uri;
  this.messageCallback = messageCallback;
}

IdpChannel.prototype = {
  /**
   * Create a hidden, sandboxed iframe for hosting the IdP's js shim.
   *
   * @param callback
   *                (function) invoked when this completes, with an error
   *                argument if there is a problem, no argument if everything is
   *                ok
   */
  open: function(callback) {
    if (this.sandbox) {
      return callback(new Error("IdP channel already open"));
    }

    var ready = this._sandboxReady.bind(this, callback);
    this.sandbox = new Sandbox(this.source, ready);
  },

  _sandboxReady: function(aCallback, aSandbox) {
    // Inject a message channel into the subframe.
    this.messagechannel = new aSandbox._frame.contentWindow.MessageChannel();
    try {
      Object.defineProperty(
        aSandbox._frame.contentWindow.wrappedJSObject,
        "rtcwebIdentityPort",
        {
          value: this.messagechannel.port2
        }
      );
    } catch (e) {
      this.close();
      aCallback(e); // oops, the IdP proxy overwrote this.. bad
      return;
    }
    this.messagechannel.port1.onmessage = function(msg) {
      this.messageCallback(msg.data);
    }.bind(this);
    this.messagechannel.port1.start();
    aCallback();
  },

  send: function(msg) {
    this.messagechannel.port1.postMessage(msg);
  },

  close: function IdpChannel_close() {
    if (this.sandbox) {
      if (this.messagechannel) {
        this.messagechannel.port1.close();
      }
      this.sandbox.free();
    }
    this.messagechannel = null;
    this.sandbox = null;
  }
};

/**
 * A message channel between the RTC PeerConnection and a designated IdP Proxy.
 *
 * @param domain (string) the domain to load up
 * @param protocol (string) Optional string for the IdP protocol
 */
function IdpProxy(domain, protocol) {
  IdpProxy.validateDomain(domain);
  IdpProxy.validateProtocol(protocol);

  this.domain = domain;
  this.protocol = protocol || "default";

  this._reset();
}

/**
 * Checks that the domain is only a domain, and doesn't contain anything else.
 * Adds it to a URI, then checks that it matches perfectly.
 */
IdpProxy.validateDomain = function(domain) {
  let message = "Invalid domain for identity provider; ";
  if (!domain || typeof domain !== "string") {
    throw new Error(message + "must be a non-zero length string");
  }

  message += "must only have a domain name and optionally a port";
  try {
    let ioService = Components.classes["@mozilla.org/network/io-service;1"]
                    .getService(Components.interfaces.nsIIOService);
    let uri = ioService.newURI('https://' + domain + '/', null, null);

    // this should trap errors
    // we could check uri.userPass, uri.path and uri.ref, but there is no need
    if (uri.hostPort !== domain) {
      throw new Error(message);
    }
  } catch (e if (e.result === Cr.NS_ERROR_MALFORMED_URI)) {
    throw new Error(message);
  }
};

/**
 * Checks that the IdP protocol is sane.  In particular, we don't want someone
 * adding relative paths (e.g., "../../myuri"), which could be used to move
 * outside of /.well-known/ and into space that they control.
 */
IdpProxy.validateProtocol = function(protocol) {
  if (!protocol) {
    return;  // falsy values turn into "default", so they are OK
  }
  let message = "Invalid protocol for identity provider; ";
  if (typeof protocol !== "string") {
    throw new Error(message + "must be a string");
  }
  if (decodeURIComponent(protocol).match(/[\/\\]/)) {
    throw new Error(message + "must not include '/' or '\\'");
  }
};

IdpProxy.prototype = {
  _reset: function() {
    this.channel = null;
    this.ready = false;

    this.counter = 0;
    this.tracking = {};
    this.pending = [];
  },

  /**
   * Get a sandboxed iframe for hosting the idp-proxy's js. Create a message
   * channel down to the frame.
   *
   * @param errorCallback (function) a callback that will be invoked if there
   *                is a fatal error starting the proxy
   */
  start: function(errorCallback) {
    if (this.channel) {
      return;
    }
    let well_known = "https://" + this.domain;
    well_known += "/.well-known/idp-proxy/" + this.protocol;
    this.channel = new IdpChannel(well_known, this._messageReceived.bind(this));
    this.channel.open(function(error) {
      if (error) {
        this.close();
        if (typeof errorCallback === "function") {
          errorCallback(error);
        }
      }
    }.bind(this));
  },

  /**
   * Send a message up to the idp proxy. This should be an RTC "SIGN" or
   * "VERIFY" message. This method adds the tracking 'id' parameter
   * automatically to the message so that the callback is only invoked for the
   * response to the message.
   *
   * The caller is responsible for ensuring that a response is received. If the
   * IdP doesn't respond, the callback simply isn't invoked.
   */
  send: function(message, callback) {
    this.start();
    if (this.ready) {
      message.id = "" + (++this.counter);
      this.tracking[message.id] = callback;
      this.channel.send(message);
    } else {
      this.pending.push({ message: message, callback: callback });
    }
  },

  /**
   * Handle a message from the IdP. This automatically sends if the message is
   * 'READY' so there is no need to track readiness state outside of this obj.
   */
  _messageReceived: function(message) {
    if (!message) {
      return;
    }
    if (message.type === "READY") {
      this.ready = true;
      this.pending.forEach(function(p) {
        this.send(p.message, p.callback);
      }, this);
      this.pending = [];
    } else if (this.tracking[message.id]) {
      var callback = this.tracking[message.id];
      delete this.tracking[message.id];
      callback(message);
    } else {
      let console = Cc["@mozilla.org/consoleservice;1"].
        getService(Ci.nsIConsoleService);
      console.logStringMessage("Received bad message from IdP: " +
                               message.id + ":" + message.type);
    }
  },

  /**
   * Performs cleanup.  The object should be OK to use again.
   */
  close: function() {
    if (!this.channel) {
      return;
    }

    // dump a message of type "ERROR" in response to all outstanding
    // messages to the IdP
    let error = { type: "ERROR" };
    Object.keys(this.tracking).forEach(function(k) {
      this.tracking[k](error);
    }, this);
    this.pending.forEach(function(p) {
      p.callback(error);
    }, this);

    this.channel.close();
    this._reset();
  },

  toString: function() {
    return this.domain + '/' + this.protocol;
  }
};

this.IdpProxy = IdpProxy;
