/*
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

var EXPORTED_SYMBOLS = ['FileLoader'];

Components.utils.import('resource://gre/modules/Services.jsm');
Components.utils.import('resource://gre/modules/Promise.jsm');
Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import('resource://gre/modules/NetUtil.jsm');

function FileLoaderSession(sessionId) {
  this.sessionId = sessionId;
  this.xhr = null;
}
FileLoaderSession.prototype = {
  abort() {
    if (this.xhr) {
      this.xhr.abort();
      this.xhr = null;
    }
  }
};


function FileLoader(swfUrl, baseUrl, refererUrl, callback) {
  this.swfUrl = swfUrl;
  this.baseUrl = baseUrl;
  this.refererUrl = refererUrl;
  this.callback = callback;
  this.activeSessions = Object.create(null);

  this.crossdomainRequestsCache = Object.create(null);
}

FileLoader.prototype = {
  load: function (data) {
    function notifyLoadFileListener(data) {
      if (!onLoadFileCallback) {
        return;
      }
      onLoadFileCallback(data);
    }

    var onLoadFileCallback = this.callback;
    var crossdomainRequestsCache = this.crossdomainRequestsCache;
    var baseUrl = this.baseUrl;
    var swfUrl = this.swfUrl;

    var url = data.url;
    var checkPolicyFile = data.checkPolicyFile;
    var sessionId = data.sessionId;
    var limit = data.limit || 0;
    var method = data.method || "GET";
    var mimeType = data.mimeType;
    var postData = data.postData || null;
    var sendReferer = canSendReferer(swfUrl, this.refererUrl);

    var session = new FileLoaderSession(sessionId);
    this.activeSessions[sessionId] = session;
    var self = this;

    var performXHR = function () {
      // Load has been aborted before we reached this point.
      if (!self.activeSessions[sessionId]) {
        return;
      }
      var xhr = session.xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"].
                              createInstance(Components.interfaces.nsIXMLHttpRequest);
      xhr.open(method, url, true);
      xhr.responseType = "moz-chunked-arraybuffer";

      if (sendReferer) {
        // Setting the referer uri, some site doing checks if swf is embedded
        // on the original page.
        xhr.setRequestHeader("Referer", self.refererUrl);
      }

      // TODO apply range request headers if limit is specified

      var lastPosition = 0;
      xhr.onprogress = function (e) {
        var position = e.loaded;
        var total = e.total;
        var data = new Uint8Array(xhr.response);
        // The event's `loaded` and `total` properties are sometimes lower than the actual
        // number of loaded bytes. In that case, increase them to that value.
        position = Math.max(position, data.byteLength);
        total = Math.max(total, data.byteLength);

        notifyLoadFileListener({callback:"loadFile", sessionId: sessionId,
          topic: "progress", array: data, loaded: position, total: total});
        lastPosition = position;
        if (limit && total >= limit) {
          xhr.abort();
        }
      };
      xhr.onreadystatechange = function(event) {
        if (xhr.readyState === 4) {
          delete self.activeSessions[sessionId];
          if (xhr.status !== 200 && xhr.status !== 0) {
            notifyLoadFileListener({callback:"loadFile", sessionId: sessionId, topic: "error", error: xhr.statusText});
          }
          notifyLoadFileListener({callback:"loadFile", sessionId: sessionId, topic: "close"});
        }
      };
      if (mimeType)
        xhr.setRequestHeader("Content-Type", mimeType);
      xhr.send(postData);
      notifyLoadFileListener({callback:"loadFile", sessionId: sessionId, topic: "open"});
    };

    canDownloadFile(url, checkPolicyFile, swfUrl, crossdomainRequestsCache).then(function () {
      performXHR();
    }, function (reason) {
      log("data access is prohibited to " + url + " from " + baseUrl);
      delete self.activeSessions[sessionId];
      notifyLoadFileListener({
        callback: "loadFile", sessionId: sessionId, topic: "error",
        error: "only original swf file or file from the same origin loading supported (XDOMAIN)"
      });
    });
  },
  abort: function(sessionId) {
    var session = this.activeSessions[sessionId];
    if (!session) {
      log("Warning: trying to abort invalid session " + sessionId);
      return;
    }
    session.abort();
    delete this.activeSessions[sessionId];
  }
};

function log(aMsg) {
  let msg = 'FileLoader.js: ' + (aMsg.join ? aMsg.join('') : aMsg);
  Services.console.logStringMessage(msg);
  dump(msg + '\n');
}

function getStringPref(pref, def) {
  try {
    return Services.prefs.getComplexValue(pref, Components.interfaces.nsISupportsString).data;
  } catch (ex) {
    return def;
  }
}

function disableXHRRedirect(xhr) {
  var listener = {
    asyncOnChannelRedirect: function(oldChannel, newChannel, flags, callback) {
      // TODO perform URL check?
      callback.onRedirectVerifyCallback(Components.results.NS_ERROR_ABORT);
    },
    getInterface: function(iid) {
      return this.QueryInterface(iid);
    },
    QueryInterface: XPCOMUtils.generateQI([Components.interfaces.nsIChannelEventSink])
  };
  xhr.channel.notificationCallbacks = listener;
}

function canSendReferer(url, refererUrl) {
  if (!refererUrl) {
    return false;
  }
  // Allow sending HTTPS referer only to HTTPS.
  var parsedUrl, parsedRefererUrl;
  try {
    parsedRefererUrl = NetUtil.newURI(refererUrl);
  } catch (ex) { /* skipping invalid urls */ }
  if (!parsedRefererUrl ||
      parsedRefererUrl.scheme.toLowerCase() !== 'https') {
    return true;
  }
  try {
    parsedUrl = NetUtil.newURI(url);
  } catch (ex) { /* skipping invalid urls */ }
  return !!parsedUrl && parsedUrl.scheme.toLowerCase() === 'https';
}

function canDownloadFile(url, checkPolicyFile, swfUrl, cache) {
  // TODO flash cross-origin request
  if (url === swfUrl) {
    // Allows downloading for the original file.
    return Promise.resolve(undefined);
  }

  // Allows downloading from the same origin.
  var parsedUrl, parsedBaseUrl;
  try {
    parsedUrl = NetUtil.newURI(url);
  } catch (ex) { /* skipping invalid urls */ }
  try {
    parsedBaseUrl = NetUtil.newURI(swfUrl);
  } catch (ex) { /* skipping invalid urls */ }

  if (parsedUrl && parsedBaseUrl &&
    parsedUrl.prePath === parsedBaseUrl.prePath) {
    return Promise.resolve(undefined);
  }

  // Additionally using internal whitelist.
  var whitelist = getStringPref('shumway.whitelist', '');
  if (whitelist && parsedUrl) {
    var whitelisted = whitelist.split(',').some(function (i) {
      return domainMatches(parsedUrl.host, i);
    });
    if (whitelisted) {
      return Promise.resolve();
    }
  }

  if (!parsedUrl || !parsedBaseUrl) {
    return Promise.reject('Invalid or non-specified URL or Base URL.');
  }

  if (!checkPolicyFile) {
    return Promise.reject('Check of the policy file is not allowed.');
  }

  // We can request crossdomain.xml.
  return fetchPolicyFile(parsedUrl.prePath + '/crossdomain.xml', cache).
    then(function (policy) {

      if (policy.siteControl === 'none') {
        throw 'Site control is set to \"none\"';
      }
      // TODO assuming master-only, there are also 'by-content-type', 'all', etc.

      var allowed = policy.allowAccessFrom.some(function (i) {
        return domainMatches(parsedBaseUrl.host, i.domain) &&
          (!i.secure || parsedBaseUrl.scheme.toLowerCase() === 'https');
      });
      if (!allowed) {
        throw 'crossdomain.xml does not contain source URL.';
      }
      return undefined;
    });
}

function domainMatches(host, pattern) {
  if (!pattern) return false;
  if (pattern === '*') return true;
  host = host.toLowerCase();
  var parts = pattern.toLowerCase().split('*');
  if (host.indexOf(parts[0]) !== 0) return false;
  var p = parts[0].length;
  for (var i = 1; i < parts.length; i++) {
    var j = host.indexOf(parts[i], p);
    if (j === -1) return false;
    p = j + parts[i].length;
  }
  return parts[parts.length - 1] === '' || p === host.length;
}

function fetchPolicyFile(url, cache) {
  if (url in cache) {
    return cache[url];
  }

  var deferred = Promise.defer();

  log('Fetching policy file at ' + url);
  var MAX_POLICY_SIZE = 8192;
  var xhr =  Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
    .createInstance(Components.interfaces.nsIXMLHttpRequest);
  xhr.open('GET', url, true);
  disableXHRRedirect(xhr);
  xhr.overrideMimeType('text/xml');
  xhr.onprogress = function (e) {
    if (e.loaded >= MAX_POLICY_SIZE) {
      xhr.abort();
      cache[url] = false;
      deferred.reject('Max policy size');
    }
  };
  xhr.onreadystatechange = function(event) {
    if (xhr.readyState === 4) {
      // TODO disable redirects
      var doc = xhr.responseXML;
      if (xhr.status !== 200 || !doc) {
        deferred.reject('Invalid HTTP status: ' + xhr.statusText);
        return;
      }
      // parsing params
      var params = doc.documentElement.childNodes;
      var policy = { siteControl: null, allowAccessFrom: []};
      for (var i = 0; i < params.length; i++) {
        switch (params[i].localName) {
          case 'site-control':
            policy.siteControl = params[i].getAttribute('permitted-cross-domain-policies');
            break;
          case 'allow-access-from':
            var access = {
              domain: params[i].getAttribute('domain'),
              security: params[i].getAttribute('security') === 'true'
            };
            policy.allowAccessFrom.push(access);
            break;
          default:
            // TODO allow-http-request-headers-from and other
            break;
        }
      }
      deferred.resolve(policy);
    }
  };
  xhr.send(null);
  return (cache[url] = deferred.promise);
}
