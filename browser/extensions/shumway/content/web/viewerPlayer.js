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

var release = true;
var SHUMWAY_ROOT = "resource://shumway/";

var viewerPlayerglobalInfo = {
  abcs: SHUMWAY_ROOT + "playerglobal/playerglobal.abcs",
  catalog: SHUMWAY_ROOT + "playerglobal/playerglobal.json"
};

var avm2Root = SHUMWAY_ROOT + "avm2/";
var builtinPath = avm2Root + "generated/builtin/builtin.abc";

window.print = function(msg) {
  console.log(msg);
};

function runSwfPlayer(flashParams) {
  var EXECUTION_MODE = Shumway.AVM2.Runtime.ExecutionMode;

  var compilerSettings = flashParams.compilerSettings;
  var sysMode = compilerSettings.sysCompiler ? EXECUTION_MODE.COMPILE : EXECUTION_MODE.INTERPRET;
  var appMode = compilerSettings.appCompiler ? EXECUTION_MODE.COMPILE : EXECUTION_MODE.INTERPRET;
  var asyncLoading = true;
  var baseUrl = flashParams.baseUrl;
  var objectParams = flashParams.objectParams;
  var movieUrl = flashParams.url;

  Shumway.frameRateOption.value = flashParams.turboMode ? 60 : -1;
  Shumway.AVM2.Verifier.enabled.value = compilerSettings.verifier;

  Shumway.createAVM2(builtinPath, viewerPlayerglobalInfo, sysMode, appMode, function (avm2) {
    function runSWF(file, buffer, baseUrl) {
      var player = new Shumway.Player.Window.WindowPlayer(window, window.parent);
      player.defaultStageColor = flashParams.bgcolor;
      player.movieParams = flashParams.movieParams;
      player.stageAlign = (objectParams && (objectParams.salign || objectParams.align)) || '';
      player.stageScale = (objectParams && objectParams.scale) || 'showall';
      player.displayParameters = flashParams.displayParameters;

      Shumway.ExternalInterfaceService.instance = player.createExternalInterfaceService();

      player.pageUrl = baseUrl;
      player.load(file, buffer);
    }
    Shumway.FileLoadingService.instance.setBaseUrl(baseUrl);
    if (asyncLoading) {
      runSWF(movieUrl, undefined, baseUrl);
    } else {
      new Shumway.BinaryFileReader(movieUrl).readAll(null, function(buffer, error) {
        if (!buffer) {
          throw "Unable to open the file " + movieUrl + ": " + error;
        }
        runSWF(movieUrl, buffer, baseUrl);
      });
    }
  });
}

var LOADER_WORKER_PATH = SHUMWAY_ROOT + 'web/worker.js';

function setupServices() {
  Shumway.Telemetry.instance = {
    reportTelemetry: function (data) {
      window.parent.postMessage({
        callback: 'reportTelemetry',
        data: data
      }, '*');
    }
  };

  Shumway.ClipboardService.instance = {
    setClipboard: function (data) {
      window.parent.postMessage({
        callback: 'setClipboard',
        data: data
      }, '*');
    }
  };

  Shumway.FileLoadingService.instance = {
    baseUrl: null,
    nextSessionId: 1, // 0 - is reserved
    sessions: [],
    createSession: function () {
      var sessionId = this.nextSessionId++;
      return this.sessions[sessionId] = {
        open: function (request) {
          var self = this;
          var path = Shumway.FileLoadingService.instance.resolveUrl(request.url);
          console.log('Session #' + sessionId + ': loading ' + path);
          window.parent.postMessage({
            callback: 'loadFileRequest',
            data: {url: path, method: request.method,
              mimeType: request.mimeType, postData: request.data,
              checkPolicyFile: request.checkPolicyFile, sessionId: sessionId}
          }, '*');
        },
        notify: function (args) {
          switch (args.topic) {
            case "open":
              this.onopen();
              break;
            case "close":
              this.onclose();
              Shumway.FileLoadingService.instance.sessions[sessionId] = null;
              console.log('Session #' + sessionId + ': closed');
              break;
            case "error":
              this.onerror && this.onerror(args.error);
              break;
            case "progress":
              console.log('Session #' + sessionId + ': loaded ' + args.loaded + '/' + args.total);
              this.onprogress && this.onprogress(args.array, {bytesLoaded: args.loaded, bytesTotal: args.total});
              break;
          }
        },
        close: function () {
          if (Shumway.FileLoadingService.instance.sessions[sessionId]) {
            // TODO send abort
          }
        }
      };
    },
    setBaseUrl: function (url) {
      Shumway.FileLoadingService.instance.baseUrl = url;
    },
    resolveUrl: function (url) {
      return new URL(url, Shumway.FileLoadingService.instance.baseUrl).href;
    },
    navigateTo: function (url, target) {
      window.parent.postMessage({
        callback: 'navigateTo',
        data: {
          url: this.resolveUrl(url),
          target: target
        }
      }, '*');
    }
  };

  // Using SpecialInflate when chrome code provides it.
  if (parent.createSpecialInflate) {
    window.SpecialInflate = function () {
      return parent.createSpecialInflate();
    };
  }

  // Using createRtmpXHR/createRtmpSocket when chrome code provides it.
  if (parent.createRtmpXHR) {
    window.createRtmpSocket = parent.createRtmpSocket;
    window.createRtmpXHR = parent.createRtmpXHR;
  }
}

window.addEventListener('message', function onWindowMessage(e) {
  var data = e.data;
  if (typeof data !== 'object' || data === null) {
    return;
  }
  switch (data.type) {
    case "loadFileResponse":
      var args = data.args;
      var session = Shumway.FileLoadingService.instance.sessions[args.sessionId];
      if (session) {
        session.notify(args);
      }
      break;
    case "runSwf":
      if (data.settings) {
        Shumway.Settings.setSettings(data.settings);
      }
      setupServices();
      runSwfPlayer(data.flashParams);

      document.body.style.backgroundColor = 'green';
      window.parent.postMessage({
        callback: 'started'
      }, '*');
      break;
  }
}, true);
