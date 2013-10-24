/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/*
 * Copyright 2013 Mozilla Foundation
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

// Extension communication object
var FirefoxCom = (function FirefoxComClosure() {
  return {
    /**
     * Creates an event that the extension is listening for and will
     * synchronously respond to.
     * NOTE: It is reccomended to use request() instead since one day we may not
     * be able to synchronously reply.
     * @param {String} action The action to trigger.
     * @param {String} data Optional data to send.
     * @return {*} The response.
     */
    requestSync: function(action, data) {
      var e = document.createEvent('CustomEvent');
      e.initCustomEvent('shumway.message', true, false,
        {action: action, data: data, sync: true});
      document.dispatchEvent(e);
      return e.detail.response;
    },
    /**
     * Creates an event that the extension is listening for and will
     * asynchronously respond by calling the callback.
     * @param {String} action The action to trigger.
     * @param {String} data Optional data to send.
     * @param {Function} callback Optional response callback that will be called
     * with one data argument.
     */
    request: function(action, data, callback) {
      var e = document.createEvent('CustomEvent');
      e.initCustomEvent('shumway.message', true, false,
        {action: action, data: data, sync: false});
      if (callback) {
        if ('nextId' in FirefoxCom.request) {
          FirefoxCom.request.nextId = 1;
        }
        var cookie = "requestId" + (FirefoxCom.request.nextId++);
        e.detail.cookie = cookie;
        e.detail.callback = true;

        document.addEventListener('shumway.response', function listener(event) {
          if (cookie !== event.detail.cookie)
            return;

          document.removeEventListener('shumway.response', listener, false);

          var response = event.detail.response;
          return callback(response);
        }, false);
      }
      return document.dispatchEvent(e);
    },
    initJS: function (callback) {
      FirefoxCom.request('externalCom', {action: 'init'});
      document.addEventListener('shumway.remote', function (e) {
        e.detail.result = callback(e.detail.functionName, e.detail.args);
      }, false);
    }
  };
})();

function fallback() {
  FirefoxCom.requestSync('fallback', null)
}

function runViewer() {
  var flashParams = JSON.parse(FirefoxCom.requestSync('getPluginParams', null));
  FileLoadingService.setBaseUrl(flashParams.baseUrl);

  movieUrl = flashParams.url;
  if (!movieUrl) {
    console.log("no movie url provided -- stopping here");
    FirefoxCom.request('endActivation', null);
    return;
  }

  movieParams = flashParams.movieParams;
  objectParams = flashParams.objectParams;
  var isOverlay = flashParams.isOverlay;
  pauseExecution = flashParams.isPausedAtStart;

  console.log("url=" + movieUrl + ";params=" + uneval(movieParams));
  if (movieParams.fmt_list && movieParams.url_encoded_fmt_stream_map) {
    // HACK removing FLVs from the fmt_list
    movieParams.fmt_list = movieParams.fmt_list.split(',').filter(function (s) {
      var fid = s.split('/')[0];
      return fid !== '5' && fid !== '34' && fid !== '35'; // more?
    }).join(',');
  }

  parseSwf(movieUrl, movieParams, objectParams);

  if (isOverlay) {
    var fallbackDiv = document.getElementById('fallback');
    fallbackDiv.className = 'enabled';
    fallbackDiv.addEventListener('click', function(e) {
      fallback();
      e.preventDefault();
    });
    var fallbackMenu = document.getElementById('fallbackMenu');
    fallbackMenu.removeAttribute('hidden');
    fallbackMenu.addEventListener('click', fallback);
  }
  var showURLMenu = document.getElementById('showURLMenu');
  showURLMenu.addEventListener('click', showURL);

  document.getElementById('copyProfileMenu').addEventListener('click', copyProfile);
}

function showURL() {
  var flashParams = JSON.parse(FirefoxCom.requestSync('getPluginParams', null));
  window.prompt("Copy to clipboard", flashParams.url);
}

function copyProfile() {
  function toArray(v) {
    var array = [];
    for (var i = 0; i < v.length; i++) {
      array.push(v[i]);
    }
    return array;
  }
  var profile = {
    loops: {counts: toArray($L), lines: $LL},
    functions: {counts: toArray($F), lines: $FL},
    allocations: {counts: toArray($A), lines: $AL}
  };
  FirefoxCom.request('unsafeSetClipboard', JSON.stringify(profile));
}

var movieUrl, movieParams, objectParams;

window.addEventListener("message", function handlerMessage(e) {
  var args = e.data;
  switch (args.callback) {
    case "loadFile":
      var session = FileLoadingService.sessions[args.sessionId];
      if (session) {
        session.notify(args);
      }
      break;
  }
}, true);

var TelemetryService = {
  reportTelemetry: function (data) {
    FirefoxCom.request('reportTelemetry', data, null);
  }
};

var FileLoadingService = {
  get baseUrl() { return movieUrl; },
  nextSessionId: 1, // 0 - is reserved
  sessions: [],
  createSession: function () {
    var sessionId = this.nextSessionId++;
    return this.sessions[sessionId] = {
      open: function (request) {
        var self = this;
        var path = FileLoadingService.resolveUrl(request.url);
        console.log('Session #' + sessionId +': loading ' + path);
        FirefoxCom.requestSync('loadFile', {url: path, method: request.method,
          mimeType: request.mimeType, postData: request.data,
          checkPolicyFile: request.checkPolicyFile, sessionId: sessionId});
      },
      notify: function (args) {
        switch (args.topic) {
          case "open": this.onopen(); break;
          case "close":
            this.onclose();
            delete FileLoadingService.sessions[sessionId];
            console.log('Session #' + sessionId +': closed');
            break;
          case "error":
            this.onerror && this.onerror(args.error);
            break;
          case "progress":
            console.log('Session #' + sessionId + ': loaded ' + args.loaded + '/' + args.total);
            this.onprogress && this.onprogress(args.array, {bytesLoaded: args.loaded, bytesTotal: args.total});
            break;
        }
      }
    };
  },
  setBaseUrl: function (url) {
    var a = document.createElement('a');
    a.href = url || '#';
    a.setAttribute('style', 'display: none;');
    document.body.appendChild(a);
    FileLoadingService.baseUrl = a.href;
    document.body.removeChild(a);
  },
  resolveUrl: function (url) {
    if (url.indexOf('://') >= 0) return url;

    var base = FileLoadingService.baseUrl;
    base = base.lastIndexOf('/') >= 0 ? base.substring(0, base.lastIndexOf('/') + 1) : '';
    if (url.indexOf('/') === 0) {
      var m = /^[^:]+:\/\/[^\/]+/.exec(base);
      if (m) base = m[0];
    }
    return base + url;
  }
};

function parseSwf(url, movieParams, objectParams) {
  var compilerSettings = JSON.parse(
    FirefoxCom.requestSync('getCompilerSettings', null));
  enableVerifier.value = compilerSettings.verifier;

  console.log("Compiler settings: " + JSON.stringify(compilerSettings));
  console.log("Parsing " + url + "...");
  function loaded() {
    FirefoxCom.request('endActivation', null);
  }

  createAVM2(builtinPath, playerGlobalPath, avm1Path,
    compilerSettings.sysCompiler ? EXECUTION_MODE.COMPILE : EXECUTION_MODE.INTERPRET,
    compilerSettings.appCompiler ? EXECUTION_MODE.COMPILE : EXECUTION_MODE.INTERPRET,
    function (avm2) {
      console.time("Initialize Renderer");
      SWF.embed(url, document, document.getElementById("viewer"), {
         url: url,
         movieParams: movieParams,
         objectParams: objectParams,
         onComplete: loaded,
         onBeforeFrame: frame
      });
  });
}

var pauseExecution = false;
var initializeFrameControl = true;
function frame(e) {
  if (initializeFrameControl) {
    // marking that movie is started
    document.body.classList.add("started");

    TelemetryService.reportTelemetry({topic: "firstFrame"});

    // skipping frame 0
    initializeFrameControl = false;
    return;
  }
  if (pauseExecution) {
    e.cancel = true;
  }
}

document.addEventListener('keydown', function (e) {
  if (e.keyCode == 119 && e.ctrlKey) { // Ctrl+F8
    pauseExecution = !pauseExecution;
  }
}, false);
