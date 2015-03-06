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

var EXPORTED_SYMBOLS = ['ShumwayCom'];

Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import('resource://gre/modules/Services.jsm');
Components.utils.import('resource://gre/modules/Promise.jsm');
Components.utils.import('resource://gre/modules/NetUtil.jsm');

Components.utils.import('chrome://shumway/content/SpecialInflate.jsm');
Components.utils.import('chrome://shumway/content/SpecialStorage.jsm');
Components.utils.import('chrome://shumway/content/RtmpUtils.jsm');

XPCOMUtils.defineLazyModuleGetter(this, 'ShumwayTelemetry',
  'resource://shumway/ShumwayTelemetry.jsm');

const MAX_USER_INPUT_TIMEOUT = 250; // ms

function getBoolPref(pref, def) {
  try {
    return Services.prefs.getBoolPref(pref);
  } catch (ex) {
    return def;
  }
}

function getStringPref(pref, def) {
  try {
    return Services.prefs.getComplexValue(pref, Components.interfaces.nsISupportsString).data;
  } catch (ex) {
    return def;
  }
}

function log(aMsg) {
  let msg = 'ShumwayCom.js: ' + (aMsg.join ? aMsg.join('') : aMsg);
  Services.console.logStringMessage(msg);
  dump(msg + '\n');
}

var ShumwayCom = {
  createAdapter: function (content, callbacks) {

    function setupComBridge(playerWindow) {
      // Creates secondary ShumwayCom adapter and sets up the forwarders from
      // the callbacks to primary adapter.
      var playerContent = playerWindow.contentWindow;
      var secondaryAdapter = ShumwayCom.createAdapter(playerContent, callbacks);
      shumwayComAdapter.onLoadFileCallback = function (arg) {
        if (secondaryAdapter.onLoadFileCallback) {
          secondaryAdapter.onLoadFileCallback(Components.utils.cloneInto(arg, playerContent));
        }
      };
      shumwayComAdapter.onExternalCallback = function (call) {
        if (secondaryAdapter.onExternalCallback) {
          return secondaryAdapter.onExternalCallback(Components.utils.cloneInto(call, playerContent));
        }
      };
      shumwayComAdapter.onSystemResourceCallback = function (id, data) {
        if (secondaryAdapter.onSystemResourceCallback) {
          secondaryAdapter.onSystemResourceCallback(id, Components.utils.cloneInto(data, playerContent));
        }
      };
      // Sets up the _onSyncMessage helper that is used from postSyncMessage of
      // the secondary adapter.
      secondaryAdapter._onSyncMessage = function (msg) {
        if (shumwayComAdapter.onSyncMessage) {
          var waivedMsg = Components.utils.waiveXrays(msg); // for cloneInto
          return shumwayComAdapter.onSyncMessage(Components.utils.cloneInto(waivedMsg, content));
        }
      };
    }

    function genPropDesc(value) {
      return {
        enumerable: true, configurable: true, writable: true, value: value
      };
    }

    // Exposing ShumwayCom object/adapter to the unprivileged content -- setting
    // up Xray wrappers.
    var shumwayComAdapter = Components.utils.createObjectIn(content, {defineAs: 'ShumwayCom'});
    Object.defineProperties(shumwayComAdapter, {
      enableDebug: genPropDesc(function enableDebug() {
        callbacks.enableDebug()
      }),
      setFullscreen: genPropDesc(function setFullscreen(value) {
        callbacks.sendMessage('setFullscreen', value, false);
      }),
      endActivation: genPropDesc(function endActivation() {
        callbacks.sendMessage('endActivation', null, false);
      }),
      fallback: genPropDesc(function fallback() {
        callbacks.sendMessage('fallback', null, false);
      }),
      getSettings: genPropDesc(function getSettings() {
        return Components.utils.cloneInto(
          callbacks.sendMessage('getSettings', null, true), content);
      }),
      getPluginParams: genPropDesc(function getPluginParams() {
        return Components.utils.cloneInto(
          callbacks.sendMessage('getPluginParams', null, true), content);
      }),
      reportIssue: genPropDesc(function reportIssue() {
        callbacks.sendMessage('reportIssue', null, false);
      }),
      externalCom: genPropDesc(function externalCom(args) {
        var result = String(callbacks.sendMessage('externalCom', args, true));
        return Components.utils.cloneInto(result, content);
      }),
      loadFile: genPropDesc(function loadFile(args) {
        callbacks.sendMessage('loadFile', args, false);
      }),
      reportTelemetry: genPropDesc(function reportTelemetry(args) {
        callbacks.sendMessage('reportTelemetry', args, false);
      }),
      setClipboard: genPropDesc(function setClipboard(args) {
        callbacks.sendMessage('setClipboard', args, false);
      }),
      navigateTo: genPropDesc(function navigateTo(args) {
        callbacks.sendMessage('navigateTo', args, false);
      }),
      userInput: genPropDesc(function userInput() {
        callbacks.sendMessage('userInput', null, true);
      }),
      loadSystemResource: genPropDesc(function loadSystemResource(id) {
        loadShumwaySystemResource(id).then(function (data) {
          if (shumwayComAdapter.onSystemResourceCallback) {
            shumwayComAdapter.onSystemResourceCallback(id,
              Components.utils.cloneInto(data, content));
          }
        });
      }),
      setupComBridge: genPropDesc(setupComBridge),
      postSyncMessage: genPropDesc(function postSyncMessage(msg) {
        return Components.utils.cloneInto(shumwayComAdapter._onSyncMessage(msg), content);
      })
    });

    Object.defineProperties(shumwayComAdapter, {
      onLoadFileCallback: genPropDesc(null),
      onExternalCallback: genPropDesc(null),
      onSystemResourceCallback: genPropDesc(null),
      onSyncMessage: genPropDesc(null)
    });

    Object.defineProperties(shumwayComAdapter, {
      createSpecialStorage: genPropDesc(function () {
        var environment = callbacks.getEnvironment();
        return SpecialStorageUtils.createWrappedSpecialStorage(content,
          environment.swfUrl, environment.privateBrowsing);
      })
    });

    // Exposing createSpecialInflate function for DEFLATE stream decoding using
    // Gecko API.
    if (SpecialInflateUtils.isSpecialInflateEnabled) {
      Object.defineProperties(shumwayComAdapter, {
        createSpecialInflate: genPropDesc(function () {
          return SpecialInflateUtils.createWrappedSpecialInflate(content);
        })
      });
    }

    // Exposing createRtmpSocket/createRtmpXHR functions to support RTMP stream
    // functionality.
    if (RtmpUtils.isRtmpEnabled) {
      Object.defineProperties(shumwayComAdapter, {
        createRtmpSocket: genPropDesc(function (params) {
          return RtmpUtils.createSocket(content, params);
        }),
        createRtmpXHR: genPropDesc(function () {
          return RtmpUtils.createXHR(content);
        })
      });
    }

    Components.utils.makeObjectPropsNormal(shumwayComAdapter);
    return shumwayComAdapter;
  },

  createActions: function (startupInfo, window, document) {
    return new ShumwayChromeActions(startupInfo, window, document);
  }
};

function loadShumwaySystemResource(id) {
  var url, type;
  switch (id) {
    case 0: // BuiltinAbc
      url = 'resource://shumway/libs/builtin.abc';
      type = 'arraybuffer';
      break;
    case 1: // PlayerglobalAbcs
      url = 'resource://shumway/playerglobal/playerglobal.abcs';
      type = 'arraybuffer';
      break;
    case 2: // PlayerglobalManifest
      url = 'resource://shumway/playerglobal/playerglobal.json';
      type = 'json';
      break;
    default:
      return Promise.reject('Unsupported system resource id');
  }

  var deferred = Promise.defer();

  var xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                              .createInstance(Components.interfaces.nsIXMLHttpRequest);
  xhr.open('GET', url, true);
  xhr.responseType = type;
  xhr.onload = function () {
    deferred.resolve(xhr.response);
  };
  xhr.send(null);

  return deferred.promise;
}

// All the privileged actions.
function ShumwayChromeActions(startupInfo, window, document) {
  this.url = startupInfo.url;
  this.objectParams = startupInfo.objectParams;
  this.movieParams = startupInfo.movieParams;
  this.baseUrl = startupInfo.baseUrl;
  this.isOverlay = startupInfo.isOverlay;
  this.embedTag = startupInfo.embedTag;
  this.isPausedAtStart = startupInfo.isPausedAtStart;
  this.window = window;
  this.document = document;
  this.externalComInitialized = false;
  this.allowScriptAccess = startupInfo.allowScriptAccess;
  this.lastUserInput = 0;
  this.crossdomainRequestsCache = Object.create(null);
  this.telemetry = {
    startTime: Date.now(),
    features: [],
    errors: []
  };

  this.onLoadFileCallback = null;
  this.onExternalCallback = null;
}

ShumwayChromeActions.prototype = {
  // The method is created for convenience of routing messages from the OOP
  // handler or remote debugger adapter. All method calls are originated from
  // the ShumwayCom adapter (see above), or from the debugger adapter.
  // See viewerWrapper.js for these usages near sendMessage calls.
  invoke: function (name, args) {
    return this[name].call(this, args);
  },

  getBoolPref: function (data) {
    if (!/^shumway\./.test(data.pref)) {
      return null;
    }
    return getBoolPref(data.pref, data.def);
  },

  getSettings: function getSettings() {
    return {
      compilerSettings: {
        appCompiler: getBoolPref('shumway.appCompiler', true),
        sysCompiler: getBoolPref('shumway.sysCompiler', false),
        verifier: getBoolPref('shumway.verifier', true)
      },
      playerSettings: {
        turboMode: getBoolPref('shumway.turboMode', false),
        hud: getBoolPref('shumway.hud', false),
        forceHidpi: getBoolPref('shumway.force_hidpi', false)
      }
    }
  },

  getPluginParams: function getPluginParams() {
    return {
      url: this.url,
      baseUrl : this.baseUrl,
      movieParams: this.movieParams,
      objectParams: this.objectParams,
      isOverlay: this.isOverlay,
      isPausedAtStart: this.isPausedAtStart,
      isDebuggerEnabled: getBoolPref('shumway.debug.enabled', false)
    };
  },

  loadFile: function loadFile(data) {
    function notifyLoadFileListener(data) {
      if (!actions.onLoadFileCallback) {
        return;
      }
      actions.onLoadFileCallback(data);
    }

    var actions = this;
    var url = data.url;
    var checkPolicyFile = data.checkPolicyFile;
    var sessionId = data.sessionId;
    var limit = data.limit || 0;
    var method = data.method || "GET";
    var mimeType = data.mimeType;
    var postData = data.postData || null;

    var win = this.window;
    var baseUrl = this.baseUrl;

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

    canDownloadFile(url, checkPolicyFile, this.url, this.crossdomainRequestsCache).then(function () {
      performXHR();
    }, function (reason) {
      log("data access is prohibited to " + url + " from " + baseUrl);
      notifyLoadFileListener({callback:"loadFile", sessionId: sessionId, topic: "error",
        error: "only original swf file or file from the same origin loading supported"});
    });
  },

  navigateTo: function (data) {
    var embedTag = this.embedTag.wrappedJSObject;
    var window = embedTag ? embedTag.ownerDocument.defaultView : this.window;
    window.open(data.url, data.target || '_self');
  },

  fallback: function(automatic) {
    automatic = !!automatic;
    var event = this.document.createEvent('CustomEvent');
    event.initCustomEvent('shumwayFallback', true, true, {
      automatic: automatic
    });
    this.window.dispatchEvent(event);
  },

  userInput: function() {
    var win = this.window;
    var winUtils = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                      .getInterface(Components.interfaces.nsIDOMWindowUtils);
    if (winUtils.isHandlingUserInput) {
      this.lastUserInput = Date.now();
    }
  },

  isUserInputInProgress: function () {
    // TODO userInput does not work for OOP
    if (!getBoolPref('shumway.userInputSecurity', true)) {
      return true;
    }

    // We don't trust our Shumway non-privileged code just yet to verify the
    // user input -- using userInput function above to track that.
    if ((Date.now() - this.lastUserInput) > MAX_USER_INPUT_TIMEOUT) {
      return false;
    }
    // TODO other security checks?
    return true;
  },

  setClipboard: function (data) {
    if (typeof data !== 'string' || !this.isUserInputInProgress()) {
      return;
    }

    let clipboard = Components.classes["@mozilla.org/widget/clipboardhelper;1"]
                                      .getService(Components.interfaces.nsIClipboardHelper);
    clipboard.copyString(data);
  },

  setFullscreen: function (enabled) {
    enabled = !!enabled;

    if (!this.isUserInputInProgress()) {
      return;
    }

    var target = this.embedTag || this.document.body;
    if (enabled) {
      target.mozRequestFullScreen();
    } else {
      target.ownerDocument.mozCancelFullScreen();
    }
  },

  endActivation: function () {
    var event = this.document.createEvent('CustomEvent');
    event.initCustomEvent('shumwayActivated', true, true, null);
    this.window.dispatchEvent(event);
  },

  reportTelemetry: function (data) {
    var topic = data.topic;
    switch (topic) {
      case 'firstFrame':
        var time = Date.now() - this.telemetry.startTime;
        ShumwayTelemetry.onFirstFrame(time);
        break;
      case 'parseInfo':
        ShumwayTelemetry.onParseInfo({
          parseTime: +data.parseTime,
          size: +data.bytesTotal,
          swfVersion: data.swfVersion|0,
          frameRate: +data.frameRate,
          width: data.width|0,
          height: data.height|0,
          bannerType: data.bannerType|0,
          isAvm2: !!data.isAvm2
        });
        break;
      case 'feature':
        var featureType = data.feature|0;
        var MIN_FEATURE_TYPE = 0, MAX_FEATURE_TYPE = 999;
        if (featureType >= MIN_FEATURE_TYPE && featureType <= MAX_FEATURE_TYPE &&
          !this.telemetry.features[featureType]) {
          this.telemetry.features[featureType] = true; // record only one feature per SWF
          ShumwayTelemetry.onFeature(featureType);
        }
        break;
      case 'error':
        var errorType = data.error|0;
        var MIN_ERROR_TYPE = 0, MAX_ERROR_TYPE = 2;
        if (errorType >= MIN_ERROR_TYPE && errorType <= MAX_ERROR_TYPE &&
          !this.telemetry.errors[errorType]) {
          this.telemetry.errors[errorType] = true; // record only one report per SWF
          ShumwayTelemetry.onError(errorType);
        }
        break;
    }
  },

  reportIssue: function (exceptions) {
    var urlTemplate = "https://bugzilla.mozilla.org/enter_bug.cgi?op_sys=All&priority=--" +
      "&rep_platform=All&target_milestone=---&version=Trunk&product=Firefox" +
      "&component=Shumway&short_desc=&comment={comment}" +
      "&bug_file_loc={url}";
    var windowUrl = this.window.parent.wrappedJSObject.location + '';
    var url = urlTemplate.split('{url}').join(encodeURIComponent(windowUrl));
    var params = {
      swf: encodeURIComponent(this.url)
    };
    getVersionInfo().then(function (versions) {
      params.versions = versions;
    }).then(function () {
      var ffbuild = params.versions.geckoVersion + ' (' + params.versions.geckoBuildID + ')';
      //params.exceptions = encodeURIComponent(exceptions);
      var comment = '+++ Initially filed via the problem reporting functionality in Shumway +++\n' +
        'Please add any further information that you deem helpful here:\n\n\n\n' +
        '----------------------\n\n' +
        'Technical Information:\n' +
        'Firefox version: ' + ffbuild + '\n' +
        'Shumway version: ' + params.versions.shumwayVersion;
      url = url.split('{comment}').join(encodeURIComponent(comment));
      this.window.open(url);
    }.bind(this));
  },

  externalCom: function (data) {
    if (!this.allowScriptAccess)
      return;

    // TODO check security ?
    var parentWindow = this.window.parent.wrappedJSObject;
    var embedTag = this.embedTag.wrappedJSObject;
    switch (data.action) {
      case 'init':
        if (this.externalComInitialized)
          return;

        this.externalComInitialized = true;
        initExternalCom(parentWindow, embedTag, this);
        return;
      case 'getId':
        return embedTag.id;
      case 'eval':
        return parentWindow.__flash__eval(data.expression);
      case 'call':
        return parentWindow.__flash__call(data.request);
      case 'register':
        return embedTag.__flash__registerCallback(data.functionName);
      case 'unregister':
        return embedTag.__flash__unregisterCallback(data.functionName);
    }
  }
};

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

function getVersionInfo() {
  var deferred = Promise.defer();
  var versionInfo = {
    geckoVersion: 'unknown',
    geckoBuildID: 'unknown',
    shumwayVersion: 'unknown'
  };
  try {
    var appInfo = Components.classes["@mozilla.org/xre/app-info;1"]
                                    .getService(Components.interfaces.nsIXULAppInfo);
    versionInfo.geckoVersion = appInfo.version;
    versionInfo.geckoBuildID = appInfo.appBuildID;
  } catch (e) {
    log('Error encountered while getting platform version info: ' + e);
  }
  var xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                              .createInstance(Components.interfaces.nsIXMLHttpRequest);
  xhr.open('GET', 'resource://shumway/version.txt', true);
  xhr.overrideMimeType('text/plain');
  xhr.onload = function () {
    try {
      // Trying to merge version.txt lines into something like:
      //   "version (sha) details"
      var lines = xhr.responseText.split(/\n/g);
      lines[1] = '(' + lines[1] + ')';
      versionInfo.shumwayVersion = lines.join(' ');
    } catch (e) {
      log('Error while parsing version info: ' + e);
    }
    deferred.resolve(versionInfo);
  };
  xhr.onerror = function () {
    log('Error while reading version info: ' + xhr.error);
    deferred.resolve(versionInfo);
  };
  xhr.send();

  return deferred.promise;
}

function initExternalCom(wrappedWindow, wrappedObject, actions) {
  var traceExternalInterface = getBoolPref('shumway.externalInterface.trace', false);
  if (!wrappedWindow.__flash__initialized) {
    wrappedWindow.__flash__initialized = true;
    wrappedWindow.__flash__toXML = function __flash__toXML(obj) {
      switch (typeof obj) {
        case 'boolean':
          return obj ? '<true/>' : '<false/>';
        case 'number':
          return '<number>' + obj + '</number>';
        case 'object':
          if (obj === null) {
            return '<null/>';
          }
          if ('hasOwnProperty' in obj && obj.hasOwnProperty('length')) {
            // array
            var xml = '<array>';
            for (var i = 0; i < obj.length; i++) {
              xml += '<property id="' + i + '">' + __flash__toXML(obj[i]) + '</property>';
            }
            return xml + '</array>';
          }
          var xml = '<object>';
          for (var i in obj) {
            xml += '<property id="' + i + '">' + __flash__toXML(obj[i]) + '</property>';
          }
          return xml + '</object>';
        case 'string':
          return '<string>' + obj.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;') + '</string>';
        case 'undefined':
          return '<undefined/>';
      }
    };
    wrappedWindow.__flash__eval = function (expr) {
      traceExternalInterface && this.console.log('__flash__eval: ' + expr);
      // allowScriptAccess protects page from unwanted swf scripts,
      // we can execute script in the page context without restrictions.
      var result = this.eval(expr);
      traceExternalInterface && this.console.log('__flash__eval (result): ' + result);
      return result;
    }.bind(wrappedWindow);
    wrappedWindow.__flash__call = function (expr) {
      traceExternalInterface && this.console.log('__flash__call (ignored): ' + expr);
    };
  }
  wrappedObject.__flash__registerCallback = function (functionName) {
    traceExternalInterface && wrappedWindow.console.log('__flash__registerCallback: ' + functionName);
    Components.utils.exportFunction(function () {
      var args = Array.prototype.slice.call(arguments, 0);
      traceExternalInterface && wrappedWindow.console.log('__flash__callIn: ' + functionName);
      var result;
      if (actions.onExternalCallback) {
        result = actions.onExternalCallback({functionName: functionName, args: args});
        traceExternalInterface && wrappedWindow.console.log('__flash__callIn (result): ' + result);
      }
      return wrappedWindow.eval(result);
    }, this, { defineAs: functionName });
  };
  wrappedObject.__flash__unregisterCallback = function (functionName) {
    traceExternalInterface && wrappedWindow.console.log('__flash__unregisterCallback: ' + functionName);
    delete this[functionName];
  };
}
