/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const {
  classes: Cc,
  interfaces: Ci,
  utils: Cu,
  results: Cr
} = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

/** This little class ensures that redirects maintain an https:// origin */
function RedirectHttpsOnly() {}

RedirectHttpsOnly.prototype = {
  asyncOnChannelRedirect: function(oldChannel, newChannel, flags, callback) {
    if (newChannel.URI.scheme !== 'https') {
      callback.onRedirectVerifyCallback(Cr.NS_ERROR_ABORT);
    } else {
      callback.onRedirectVerifyCallback(Cr.NS_OK);
    }
  },

  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannelEventSink])
};

/** This class loads a resource into a single string. ResourceLoader.load() is
 * the entry point. */
function ResourceLoader(res, rej) {
  this.resolve = res;
  this.reject = rej;
  this.data = '';
}

/** Loads the identified https:// URL. */
ResourceLoader.load = function(uri, doc) {
  return new Promise((resolve, reject) => {
    let listener = new ResourceLoader(resolve, reject);
    let ioService = Cc['@mozilla.org/network/io-service;1']
      .getService(Ci.nsIIOService);
    let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    // the '2' identifies this as a script load
    let ioChannel = ioService.newChannelFromURI2(uri, doc, doc.nodePrincipal,
                                                 systemPrincipal, 0,
                                                 Ci.nsIContentPolicy.TYPE_SCRIPT);

    ioChannel.loadGroup = doc.documentLoadGroup.QueryInterface(Ci.nsILoadGroup);
    ioChannel.notificationCallbacks = new RedirectHttpsOnly();
    ioChannel.asyncOpen(listener, null);
  });
};

ResourceLoader.prototype = {
  onDataAvailable: function(request, context, input, offset, count) {
    let stream = Cc['@mozilla.org/scriptableinputstream;1']
      .createInstance(Ci.nsIScriptableInputStream);
    stream.init(input);
    this.data += stream.read(count);
  },

  onStartRequest: function (request, context) {},

  onStopRequest: function(request, context, status) {
    if (Components.isSuccessCode(status)) {
      var statusCode = request.QueryInterface(Ci.nsIHttpChannel).responseStatus;
      if (statusCode === 200) {
        this.resolve({ request: request, data: this.data });
      } else {
        this.reject(new Error('Non-200 response from server: ' + statusCode));
      }
    } else {
      this.reject(new Error('Load failed: ' + status));
    }
  },

  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStreamListener])
};

/**
 * A simple implementation of the WorkerLocation interface.
 */
function createLocationFromURI(uri) {
  return {
    href: uri.spec,
    protocol: uri.scheme + ':',
    host: uri.host + ((uri.port >= 0) ?
                      (':' + uri.port) : ''),
    port: uri.port,
    hostname: uri.host,
    pathname: uri.path.replace(/[#\?].*/, ''),
    search: uri.path.replace(/^[^\?]*/, '').replace(/#.*/, ''),
    hash: uri.hasRef ? ('#' + uri.ref) : '',
    origin: uri.prePath,
    toString: function() {
      return uri.spec;
    }
  };
}

/**
 * A javascript sandbox for running an IdP.
 *
 * @param domain (string) the domain of the IdP
 * @param protocol (string?) the protocol of the IdP [default: 'default']
 * @param win (obj) the current window
 * @throws if the domain or protocol aren't valid
 */
function IdpSandbox(domain, protocol, win) {
  this.source = IdpSandbox.createIdpUri(domain, protocol || "default");
  this.active = null;
  this.sandbox = null;
  this.window = win;
}

IdpSandbox.checkDomain = function(domain) {
  if (!domain || typeof domain !== 'string') {
    throw new Error('Invalid domain for identity provider: ' +
                    'must be a non-zero length string');
  }
};

/**
 * Checks that the IdP protocol is superficially sane.  In particular, we don't
 * want someone adding relative paths (e.g., '../../myuri'), which could be used
 * to move outside of /.well-known/ and into space that they control.
 */
IdpSandbox.checkProtocol = function(protocol) {
  let message = 'Invalid protocol for identity provider: ';
  if (!protocol || typeof protocol !== 'string') {
    throw new Error(message + 'must be a non-zero length string');
  }
  if (decodeURIComponent(protocol).match(/[\/\\]/)) {
    throw new Error(message + "must not include '/' or '\\'");
  }
};

/**
 * Turns a domain and protocol into a URI.  This does some aggressive checking
 * to make sure that we aren't being fooled somehow.  Throws on fooling.
 */
IdpSandbox.createIdpUri = function(domain, protocol) {
  IdpSandbox.checkDomain(domain);
  IdpSandbox.checkProtocol(protocol);

  let message = 'Invalid IdP parameters: ';
  try {
    let wkIdp = 'https://' + domain + '/.well-known/idp-proxy/' + protocol;
    let ioService = Components.classes['@mozilla.org/network/io-service;1']
                    .getService(Ci.nsIIOService);
    let uri = ioService.newURI(wkIdp, null, null);

    if (uri.hostPort !== domain) {
      throw new Error(message + 'domain is invalid');
    }
    if (uri.path.indexOf('/.well-known/idp-proxy/') !== 0) {
      throw new Error(message + 'must produce a /.well-known/idp-proxy/ URI');
    }

    return uri;
  } catch (e if (typeof e.result !== 'undefined' &&
                 e.result === Cr.NS_ERROR_MALFORMED_URI)) {
    throw new Error(message + 'must produce a valid URI');
  }
};

IdpSandbox.prototype = {
  isSame: function(domain, protocol) {
    return this.source.spec === IdpSandbox.createIdpUri(domain, protocol).spec;
  },

  start: function() {
    if (!this.active) {
      this.active = ResourceLoader.load(this.source, this.window.document)
        .then(result => this._createSandbox(result));
    }
    return this.active;
  },

  // Provides the sandbox with some useful facilities.  Initially, this is only
  // a minimal set; it is far easier to add more as the need arises, than to
  // take them back if we discover a mistake.
  _populateSandbox: function(uri) {
    this.sandbox.location = Cu.cloneInto(createLocationFromURI(uri),
                                         this.sandbox,
                                         { cloneFunctions: true });
  },

  _createSandbox: function(result) {
    let principal = Services.scriptSecurityManager
      .getChannelResultPrincipal(result.request);

    this.sandbox = Cu.Sandbox(principal, {
      sandboxName: 'IdP-' + this.source.host,
      wantComponents: false,
      wantExportHelpers: false,
      wantGlobalProperties: [
        'indexedDB', 'XMLHttpRequest', 'TextEncoder', 'TextDecoder',
        'URL', 'URLSearchParams', 'atob', 'btoa', 'Blob', 'crypto',
        'rtcIdentityProvider', 'fetch'
      ]
    });
    let registrar = this.sandbox.rtcIdentityProvider;
    if (!Cu.isXrayWrapper(registrar)) {
      throw new Error('IdP setup failed');
    }

    // have to use the ultimate URI, not the starting one to avoid
    // that origin stealing from the one that redirected to it
    this._populateSandbox(result.request.URI);
    try {
      Cu.evalInSandbox(result.data, this.sandbox,
                       'latest', result.request.URI.spec, 1);
    } catch (e) {
      // These can be passed straight on, because they are explicitly labelled
      // as being IdP errors by the IdP and we drop line numbers as a result.
      if (e.name === 'IdpError' || e.name === 'IdpLoginError') {
        throw e;
      }
      this._logError(e);
      throw new Error('Error in IdP, check console for details');
    }

    if (!registrar.idp) {
      throw new Error('IdP failed to call rtcIdentityProvider.register()');
    }
    return registrar;
  },

  // Capture all the details from the error and log them to the console.  This
  // can't rethrow anything else because that could leak information about the
  // internal workings of the IdP across origins.
  _logError: function(e) {
    let winID = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils).currentInnerWindowID;
    let scriptError = Cc["@mozilla.org/scripterror;1"]
        .createInstance(Ci.nsIScriptError);
    scriptError.initWithWindowID(e.message, e.fileName, null,
                                 e.lineNumber, e.columnNumber,
                                 Ci.nsIScriptError.errorFlag,
                                 "content javascript", winID);
    let consoleService = Cc['@mozilla.org/consoleservice;1']
        .getService(Ci.nsIConsoleService);
    consoleService.logMessage(scriptError);
  },

  stop: function() {
    if (this.sandbox) {
      Cu.nukeSandbox(this.sandbox);
    }
    this.sandbox = null;
    this.active = null;
  },

  toString: function() {
    return this.source.spec;
  }
};

this.EXPORTED_SYMBOLS = ['IdpSandbox'];
this.IdpSandbox = IdpSandbox;
