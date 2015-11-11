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

Components.utils.import('resource://gre/modules/NetUtil.jsm');
Components.utils.import('resource://gre/modules/Promise.jsm');
Components.utils.import('resource://gre/modules/Services.jsm');
Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');

Components.utils.import('chrome://shumway/content/SpecialInflate.jsm');
Components.utils.import('chrome://shumway/content/SpecialStorage.jsm');
Components.utils.import('chrome://shumway/content/RtmpUtils.jsm');
Components.utils.import('chrome://shumway/content/ExternalInterface.jsm');
Components.utils.import('chrome://shumway/content/FileLoader.jsm');
Components.utils.import('chrome://shumway/content/LocalConnection.jsm');

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

function getCharPref(pref, def) {
  try {
    return Services.prefs.getCharPref(pref);
  } catch (ex) {
    return def;
  }
}

function log(aMsg) {
  let msg = 'ShumwayCom.js: ' + (aMsg.join ? aMsg.join('') : aMsg);
  Services.console.logStringMessage(msg);
  dump(msg + '\n');
}

function sanitizeTelemetryArgs(args) {
  var request = {
    topic: String(args.topic)
  };
  switch (request.topic) {
    case 'firstFrame':
      break;
    case 'parseInfo':
      request.info = {
        parseTime: +args.parseTime,
        size: +args.bytesTotal,
        swfVersion: args.swfVersion | 0,
        frameRate: +args.frameRate,
        width: args.width | 0,
        height: args.height | 0,
        bannerType: args.bannerType | 0,
        isAvm2: !!args.isAvm2
      };
      break;
    case 'feature':
      request.featureType = args.feature | 0;
      break;
    case 'loadResource':
      request.resultType = args.resultType | 0;
      break;
    case 'error':
      request.errorType = args.error | 0;
      break;
  }
  return request;
}

function sanitizeLoadFileArgs(args) {
  return {
    url: String(args.url || ''),
    checkPolicyFile: !!args.checkPolicyFile,
    sessionId: +args.sessionId,
    limit: +args.limit || 0,
    mimeType: String(args.mimeType || ''),
    method: (args.method + '') || 'GET',
    postData: args.postData || null
  };
}

function sanitizeExternalComArgs(args) {
  var request = {
    action: String(args.action)
  };
  switch (request.action) {
    case 'eval':
      request.expression = String(args.expression);
      break;
    case 'call':
      request.expression = String(args.request);
      break;
    case 'register':
    case 'unregister':
      request.functionName = String(args.functionName);
      break;
  }
  return request;
}

var cloneIntoFromContent = (function () {
  // waiveXrays are used due to bug 1150771, checking if we are affected
  // TODO remove workaround after Firefox 40 is released (2015-08-11)
  let sandbox1 = new Components.utils.Sandbox(null);
  let sandbox2 = new Components.utils.Sandbox(null);
  let arg = Components.utils.evalInSandbox('({buf: new ArrayBuffer(2)})', sandbox1);
  let clonedArg = Components.utils.cloneInto(arg, sandbox2);
  if (!Components.utils.waiveXrays(clonedArg).buf) {
    return function (obj, contentSandbox) {
      return Components.utils.cloneInto(
        Components.utils.waiveXrays(obj), contentSandbox);
    };
  }

  return function (obj, contentSandbox) {
    return Components.utils.cloneInto(obj, contentSandbox);
  };
})();

var ShumwayEnvironment = {
  DEBUG: 'debug',
  DEVELOPMENT: 'dev',
  RELEASE: 'release',
  TEST: 'test'
};

var ShumwayCom = {
  environment: getCharPref('shumway.environment', 'dev'),
  
  createAdapter: function (content, callbacks, hooks) {
    // Exposing ShumwayCom object/adapter to the unprivileged content -- setting
    // up Xray wrappers.
    var wrapped = {
      environment: ShumwayCom.environment,
    
      enableDebug: function () {
        callbacks.enableDebug()
      },

      fallback: function () {
        callbacks.sendMessage('fallback', null, false);
      },

      getSettings: function () {
        return Components.utils.cloneInto(
          callbacks.sendMessage('getSettings', null, true), content);
      },

      getPluginParams: function () {
        return Components.utils.cloneInto(
          callbacks.sendMessage('getPluginParams', null, true), content);
      },

      reportIssue: function () {
        callbacks.sendMessage('reportIssue', null, false);
      },

      reportTelemetry: function (args) {
        var request = sanitizeTelemetryArgs(args);
        callbacks.sendMessage('reportTelemetry', request, false);
      },

      setupGfxComBridge: function (gfxWindow) {
        // Creates ShumwayCom adapter for the gfx iframe exposing only subset
        // of the privileged function. Removing Xrays to setup the ShumwayCom
        // property and for usage as a sandbox for cloneInto operations.
        var gfxContent = gfxWindow.contentWindow.wrappedJSObject;
        ShumwayCom.createGfxAdapter(gfxContent, callbacks, hooks);

        setupUserInput(gfxWindow.contentWindow, callbacks);
      },

      setupPlayerComBridge: function (playerWindow) {
        // Creates ShumwayCom adapter for the player iframe exposing only subset
        // of the privileged function. Removing Xrays to setup the ShumwayCom
        // property and for usage as a sandbox for cloneInto operations.
        var playerContent = playerWindow.contentWindow.wrappedJSObject;
        ShumwayCom.createPlayerAdapter(playerContent, callbacks, hooks);
      }
    };

    var shumwayComAdapter = Components.utils.cloneInto(wrapped, content, {cloneFunctions:true});
    content.ShumwayCom = shumwayComAdapter;
  },

  createGfxAdapter: function (content, callbacks, hooks) {
    // Exposing ShumwayCom object/adapter to the unprivileged content -- setting
    // up Xray wrappers.
    var wrapped = {
      environment: ShumwayCom.environment,

      setFullscreen: function (value) {
        value = !!value;
        callbacks.sendMessage('setFullscreen', value, false);
      },

      reportTelemetry: function (args) {
        var request = sanitizeTelemetryArgs(args);
        callbacks.sendMessage('reportTelemetry', request, false);
      },

      postAsyncMessage: function (msg) {
        if (hooks.onPlayerAsyncMessageCallback) {
          hooks.onPlayerAsyncMessageCallback(msg);
        }
      },

      setSyncMessageCallback: function (callback) {
        if (typeof callback !== 'function') {
          log('error: attempt to set non-callable as callback in setSyncMessageCallback');
          return;
        }
        hooks.onGfxSyncMessageCallback = function (msg, sandbox) {
          var reclonedMsg = cloneIntoFromContent(msg, content);
          var result = callback(reclonedMsg);
          return cloneIntoFromContent(result, sandbox);
        };
      },

      setAsyncMessageCallback: function (callback) {
        if (typeof callback !== 'function') {
          log('error: attempt to set non-callable as callback in setAsyncMessageCallback');
          return;
        }
        hooks.onGfxAsyncMessageCallback = function (msg) {
          var reclonedMsg = cloneIntoFromContent(msg, content);
          callback(reclonedMsg);
        };
      }
    };
    
    if (ShumwayCom.environment === ShumwayEnvironment.TEST) {
      wrapped.processFrame = function () {
        callbacks.sendMessage('processFrame');
      };
      
      wrapped.processFSCommand = function (command, args) {
        callbacks.sendMessage('processFSCommand', command, args);
      };
      
      wrapped.setScreenShotCallback = function (callback) {
        callbacks.sendMessage('setScreenShotCallback', callback);
      };
    }

    var shumwayComAdapter = Components.utils.cloneInto(wrapped, content, {cloneFunctions:true});
    content.ShumwayCom = shumwayComAdapter;
  },

  createPlayerAdapter: function (content, callbacks, hooks) {
    // Exposing ShumwayCom object/adapter to the unprivileged content -- setting
    // up Xray wrappers.
    var wrapped = {
      environment: ShumwayCom.environment,
    
      externalCom: function (args) {
        var request = sanitizeExternalComArgs(args);
        var result = String(callbacks.sendMessage('externalCom', request, true));
        return result;
      },

      loadFile: function (args) {
        var request = sanitizeLoadFileArgs(args);
        callbacks.sendMessage('loadFile', request, false);
      },

      abortLoad: function (sessionId) {
        sessionId = sessionId|0;
        callbacks.sendMessage('abortLoad', sessionId, false);
      },

      reportTelemetry: function (args) {
        var request = sanitizeTelemetryArgs(args);
        callbacks.sendMessage('reportTelemetry', request, false);
      },

      setClipboard: function (args) {
        if (typeof args !== 'string') {
          return; // ignore non-string argument
        }
        callbacks.sendMessage('setClipboard', args, false);
      },

      navigateTo: function (args) {
        var request = {
          url: String(args.url || ''),
          target: String(args.target || '')
        };
        callbacks.sendMessage('navigateTo', request, false);
      },

      loadSystemResource: function (id) {
        loadShumwaySystemResource(id).then(function (data) {
          if (onSystemResourceCallback) {
            onSystemResourceCallback(id, Components.utils.cloneInto(data, content));
          }
        });
      },

      sendSyncMessage: function (msg) {
        var result;
        if (hooks.onGfxSyncMessageCallback) {
          result = hooks.onGfxSyncMessageCallback(msg, content);
        }
        return result;
      },

      postAsyncMessage: function (msg) {
        if (hooks.onGfxAsyncMessageCallback) {
          hooks.onGfxAsyncMessageCallback(msg);
        }
      },

      setAsyncMessageCallback: function (callback) {
        if (typeof callback !== 'function') {
          log('error: attempt to set non-callable as callback in setAsyncMessageCallback');
          return;
        }
        hooks.onPlayerAsyncMessageCallback = function (msg) {
          var reclonedMsg = cloneIntoFromContent(msg, content);
          callback(reclonedMsg);
        };
      },

      createSpecialStorage: function () {
        var environment = callbacks.getEnvironment();
        return SpecialStorageUtils.createWrappedSpecialStorage(content,
          environment.swfUrl, environment.privateBrowsing);
      },

      getWeakMapKeys: function (weakMap) {
        var keys = ThreadSafeChromeUtils.nondeterministicGetWeakMapKeys(weakMap);
        var result = new content.Array();
        keys.forEach(function (key) {
          result.push(key);
        });
        return result;
      },

      setLoadFileCallback: function (callback) {
        if (callback !== null && typeof callback !== 'function') {
          return;
        }
        onLoadFileCallback = callback;
      },
      setExternalCallback: function (callback) {
        if (callback !== null && typeof callback !== 'function') {
          return;
        }
        onExternalCallback = callback;
      },
      setSystemResourceCallback: function (callback) {
        if (callback !== null && typeof callback !== 'function') {
          return;
        }
        onSystemResourceCallback = callback;
      },

      getLocalConnectionService: function() {
        if (!wrappedLocalConnectionService) {
          wrappedLocalConnectionService = new LocalConnectionService(content,
                                                                     callbacks.getEnvironment());
        }
        return wrappedLocalConnectionService;
      }
    };

    // Exposing createSpecialInflate function for DEFLATE stream decoding using
    // Gecko API.
    if (SpecialInflateUtils.isSpecialInflateEnabled) {
      wrapped.createSpecialInflate = function () {
        return SpecialInflateUtils.createWrappedSpecialInflate(content);
      };
    }

    // Exposing createRtmpSocket/createRtmpXHR functions to support RTMP stream
    // functionality.
    if (RtmpUtils.isRtmpEnabled) {
      wrapped.createRtmpSocket = function (params) {
        return RtmpUtils.createSocket(content, params);
      };
      wrapped.createRtmpXHR = function () {
        return RtmpUtils.createXHR(content);
      };
    }

    if (ShumwayCom.environment === ShumwayEnvironment.TEST) {
      wrapped.print = function(msg) {
        callbacks.sendMessage('print', msg);
      }
    }

    var onSystemResourceCallback = null;
    var onExternalCallback = null;
    var onLoadFileCallback = null;

    var wrappedLocalConnectionService = null;

    hooks.onLoadFileCallback = function (arg) {
      if (onLoadFileCallback) {
        onLoadFileCallback(Components.utils.cloneInto(arg, content));
      }
    };
    hooks.onExternalCallback = function (call) {
      if (onExternalCallback) {
        return onExternalCallback(Components.utils.cloneInto(call, content));
      }
    };

    var shumwayComAdapter = Components.utils.cloneInto(wrapped, content, {cloneFunctions:true});
    content.ShumwayCom = shumwayComAdapter;
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

function setupUserInput(contentWindow, callbacks) {
  function notifyUserInput() {
    callbacks.sendMessage('userInput', null, true);
  }

  // Ignoring the untrusted events by providing the 4th argument for addEventListener.
  contentWindow.document.addEventListener('mousedown', notifyUserInput, true, false);
  contentWindow.document.addEventListener('mouseup', notifyUserInput, true, false);
  contentWindow.document.addEventListener('keydown', notifyUserInput, true, false);
  contentWindow.document.addEventListener('keyup', notifyUserInput, true, false);
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
  this.initStartTime = startupInfo.initStartTime;
  this.window = window;
  this.document = document;
  this.allowScriptAccess = startupInfo.allowScriptAccess;
  this.lastUserInput = 0;
  this.telemetry = {
    startTime: Date.now(),
    features: [],
    errors: []
  };

  this.fileLoader = new FileLoader(startupInfo.url, startupInfo.baseUrl, startupInfo.refererUrl,
    function (args) { this.onLoadFileCallback(args); }.bind(this));
  this.onLoadFileCallback = null;

  this.externalInterface = null;
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
      initStartTime: this.initStartTime,
      isDebuggerEnabled: getBoolPref('shumway.debug.enabled', false)
    };
  },

  loadFile: function loadFile(data) {
    this.fileLoader.load(data);
  },

  abortLoad: function abortLoad(sessionId) {
    this.fileLoader.abort(sessionId);
  },

  navigateTo: function (data) {
    // Our restrictions are a little bit different from Flash's: let's enable
    // only http(s) and only when script execution is allowed.
    // See https://helpx.adobe.com/flash/kb/control-access-scripts-host-web.html
    var url = data.url || 'about:blank';
    var target = data.target || '_self';
    var isWhitelistedProtocol = /^(http|https):\/\//i.test(url);
    if (!isWhitelistedProtocol || !this.allowScriptAccess) {
      return;
    }
    // ...and only when user input is in-progress.
    if (!this.isUserInputInProgress()) {
      return;
    }
    var embedTag = this.embedTag;
    var window = embedTag ? embedTag.ownerDocument.defaultView : this.window;
    window.open(url, target);
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
    // Recording time of last user input for isUserInputInProgress below.
    this.lastUserInput = Date.now();
  },

  isUserInputInProgress: function () {
    // We don't trust our Shumway non-privileged code just yet to verify the
    // user input -- using userInput function above to track that.
    if ((Date.now() - this.lastUserInput) > MAX_USER_INPUT_TIMEOUT) {
      return false;
    }
    // TODO other security checks?
    return true;
  },

  setClipboard: function (data) {
    if (!this.isUserInputInProgress()) {
      return;
    }

    let clipboard = Components.classes["@mozilla.org/widget/clipboardhelper;1"]
                                      .getService(Components.interfaces.nsIClipboardHelper);
    clipboard.copyString(data);
  },

  setFullscreen: function (enabled) {
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

  reportTelemetry: function (request) {
    switch (request.topic) {
      case 'firstFrame':
        var time = Date.now() - this.telemetry.startTime;
        ShumwayTelemetry.onFirstFrame(time);
        break;
      case 'parseInfo':
        ShumwayTelemetry.onParseInfo(request.info);
        break;
      case 'feature':
        var featureType = request.featureType;
        var MIN_FEATURE_TYPE = 0, MAX_FEATURE_TYPE = 999;
        if (featureType >= MIN_FEATURE_TYPE && featureType <= MAX_FEATURE_TYPE &&
          !this.telemetry.features[featureType]) {
          this.telemetry.features[featureType] = true; // record only one feature per SWF
          ShumwayTelemetry.onFeature(featureType);
        }
        break;
      case 'loadResource':
        var resultType = request.resultType;
        var MIN_RESULT_TYPE = 0, MAX_RESULT_TYPE = 10;
        if (resultType >= MIN_RESULT_TYPE && resultType <= MAX_RESULT_TYPE) {
          ShumwayTelemetry.onLoadResource(resultType);
        }
        break;
      case 'error':
        var errorType = request.errorType;
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
    var windowUrl = this.window.parent.location.href + '';
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

    // TODO check more security stuff ?
    if (!this.externalInterface) {
      var parentWindow = this.window.parent; // host page -- parent of PlayPreview frame
      var embedTag = this.embedTag;
      this.externalInterface = new ExternalInterface(parentWindow, embedTag, function (call) {
        return this.onExternalCallback(call);
      }.bind(this));
    }

    return this.externalInterface.processAction(data);
  },
  
  postMessage: function (type, data) {
    var embedTag = this.embedTag;
    var event = embedTag.ownerDocument.createEvent('CustomEvent');
    var detail = Components.utils.cloneInto({ type: type, data: data }, embedTag.ownerDocument.wrappedJSObject);
    event.initCustomEvent('message', false, false, detail);
    embedTag.dispatchEvent(event);
  },
  
  processFrame: function () {
    this.postMessage('processFrame');
  },

  processFSCommand: function (command, data) {
    this.postMessage('processFSCommand', { command: command, data: data });
  },

  print: function (msg) {
    this.postMessage('print', msg);
  },

  setScreenShotCallback: function (callback) {
    var embedTag = this.embedTag;
    Components.utils.exportFunction(function () {
      // `callback` can be wrapped in a CPOW and thus cause a slow synchronous cross-process operation.
      var result = callback();
      return Components.utils.cloneInto(result, embedTag.ownerDocument);
    }, embedTag.wrappedJSObject, {defineAs: 'getCanvasData'});
  }
};

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
