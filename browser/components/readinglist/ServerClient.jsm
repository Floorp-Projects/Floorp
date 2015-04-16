/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// The client used to access the ReadingList server.

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "RESTRequest", "resource://services-common/rest.js");
XPCOMUtils.defineLazyModuleGetter(this, "CommonUtils", "resource://services-common/utils.js");
XPCOMUtils.defineLazyModuleGetter(this, "fxAccounts", "resource://gre/modules/FxAccounts.jsm");

let log = Log.repository.getLogger("readinglist.serverclient");

const OAUTH_SCOPE = "readinglist"; // The "scope" on the oauth token we request.

this.EXPORTED_SYMBOLS = [
  "ServerClient",
];

// utf-8 joy. rest.js, which we use for the underlying requests, does *not*
// encode the request as utf-8 even though it wants to know the encoding.
// It does, however, explicitly decode the response.  This seems insane, but is
// what it is.
// The end result being we need to utf-8 the request and let the response take
// care of itself.
function objectToUTF8Json(obj) {
  // FTR, unescape(encodeURIComponent(JSON.stringify(obj))) also works ;)
  return CommonUtils.encodeUTF8(JSON.stringify(obj));
}

function ServerClient(fxa = fxAccounts) {
  this.fxa = fxa;
}

ServerClient.prototype = {

  request(options) {
    return this._request(options.path, options.method, options.body, options.headers);
  },

  get serverURL() {
    return Services.prefs.getCharPref("readinglist.server");
  },

  _getURL(path) {
    let result = this.serverURL;
    // we expect the path to have a leading slash, so remove any trailing
    // slashes on the pref.
    if (result.endsWith("/")) {
      result = result.slice(0, -1);
    }
    return result + path;
  },

  // Hook points for testing.
  _getToken() {
    // Assume token-caching is in place - if it's not we should avoid doing
    // this each request.
    return this.fxa.getOAuthToken({scope: OAUTH_SCOPE});
  },

  _removeToken(token) {
    return this.fxa.removeCachedOAuthToken({token});
  },

  // Converts an error from the RESTRequest object to an error we export.
  _convertRestError(error) {
    return error; // XXX - errors?
  },

  // Converts an error from a try/catch handler to an error we export.
  _convertJSError(error) {
    return error; // XXX - errors?
  },

  /*
   * Perform a request - handles authentication
   */
  _request: Task.async(function* (path, method, body, headers) {
    let token = yield this._getToken();
    let response = yield this._rawRequest(path, method, body, headers, token);
    log.debug("initial request got status ${status}", response);
    if (response.status == 401) {
      // an auth error - assume our token has expired or similar.
      this._removeToken(token);
      token = yield this._getToken();
      response = yield this._rawRequest(path, method, body, headers, token);
      log.debug("retry of request got status ${status}", response);
    }
    return response;
  }),

  /*
   * Perform a request *without* abstractions such as auth etc
   *
   * On success (which *includes* non-200 responses) returns an object like:
   * {
   *   status: 200, # http status code
   *   headers: {}, # header values keyed by header name.
   *   body: {},    # parsed json
   }
   */

  _rawRequest(path, method, body, headers, oauthToken) {
    return new Promise((resolve, reject) => {
      let url = this._getURL(path);
      log.debug("dispatching request to", url);
      let request = new RESTRequest(url);
      method = method.toUpperCase();

      request.setHeader("Accept", "application/json");
      request.setHeader("Content-Type", "application/json; charset=utf-8");
      request.setHeader("Authorization", "Bearer " + oauthToken);
      // and additional header specified for this request.
      if (headers) {
        for (let [headerName, headerValue] in Iterator(headers)) {
          log.trace("Caller specified header: ${headerName}=${headerValue}", {headerName, headerValue});
          request.setHeader(headerName, headerValue);
        }
      }

      request.onComplete = error => {
        // Although the server API docs say the "Backoff" header is on
        // successful responses while "Retry-After" is on error responses, we
        // just look for them both in both cases (as the scheduler makes no
        // distinction)
        let response = request.response;
        if (response && response.headers) {
          let backoff = response.headers["backoff"] || response.headers["retry-after"];
          if (backoff) {
            let numeric = backoff.toLowerCase() == "none" ? 0 :
                          parseInt(backoff, 10);
            if (isNaN(numeric)) {
              log.info("Server requested unrecognized backoff", backoff);
            } else if (numeric > 0) {
              log.info("Server requested backoff", numeric);
              Services.obs.notifyObservers(null, "readinglist:backoff-requested", String(numeric));
            }
          }
        }
        if (error) {
          return reject(this._convertRestError(error));
        }

        log.debug("received response status: ${status} ${statusText}", response);
        // Handle response status codes we know about
        let result = {
          status: response.status,
          headers: response.headers
        };
        try {
          if (response.body) {
            result.body = JSON.parse(response.body);
          }
        } catch (e) {
          log.debug("Response is not JSON. First 1024 chars: |${body}|",
                    { body: response.body.substr(0, 1024) });
          // We don't reject due to this (and don't even make a huge amount of
          // log noise - eg, a 50X error from a load balancer etc may not write
          // JSON.
        }

        resolve(result);
      }
      // We are assuming the body has already been decoded and thus contains
      // unicode, but the server expects utf-8. encodeURIComponent does that.
      request.dispatch(method, objectToUTF8Json(body));
    });
  },
};
