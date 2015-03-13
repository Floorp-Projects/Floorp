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

function FileLoader(swfUrl, baseUrl, callback) {
  this.swfUrl = swfUrl;
  this.baseUrl = baseUrl;
  this.callback = callback;

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


    var performXHR = function () {
      var xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
        .createInstance(Components.interfaces.nsIXMLHttpRequest);
      xhr.open(method, url, true);
      xhr.responseType = "moz-chunked-arraybuffer";

      if (baseUrl) {
        // Setting the referer uri, some site doing checks if swf is embedded
        // on the original page.
        xhr.setRequestHeader("Referer", baseUrl);
      }

      // TODO apply range request headers if limit is specified

      var lastPosition = 0;
      xhr.onprogress = function (e) {
        var position = e.loaded;
        var data = new Uint8Array(xhr.response);
        notifyLoadFileListener({callback:"loadFile", sessionId: sessionId,
          topic: "progress", array: data, loaded: position, total: e.total});
        lastPosition = position;
        if (limit && e.total >= limit) {
          xhr.abort();
        }
      };
      xhr.onreadystatechange = function(event) {
        if (xhr.readyState === 4) {
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
      notifyLoadFileListener({callback:"loadFile", sessionId: sessionId, topic: "error",
        error: "only original swf file or file from the same origin loading supported"});
    });
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
