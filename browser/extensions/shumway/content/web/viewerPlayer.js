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

  Shumway.createAVM2(Shumway.AVM2LoadLibrariesFlags.Builtin | Shumway.AVM2LoadLibrariesFlags.Playerglobal, sysMode, appMode).then(function (avm2) {
    function runSWF(file, buffer, baseUrl) {
      var gfxService = new Shumway.Player.Window.WindowGFXService(window, window.parent);
      var player = new Shumway.Player.Player(gfxService, flashParams.env);
      player.defaultStageColor = flashParams.bgcolor;
      player.movieParams = flashParams.movieParams;
      player.stageAlign = (objectParams && (objectParams.salign || objectParams.align)) || '';
      player.stageScale = (objectParams && objectParams.scale) || 'showall';
      player.displayParameters = flashParams.displayParameters;

      player.pageUrl = baseUrl;
      player.load(file, buffer);
    }

    Shumway.FileLoadingService.instance.init(baseUrl);
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

function setupServices() {
  Shumway.Telemetry.instance = new Shumway.Player.ShumwayComTelemetryService();
  Shumway.ExternalInterfaceService.instance = new Shumway.Player.ShumwayComExternalInterface();
  Shumway.ClipboardService.instance = new Shumway.Player.ShumwayComClipboardService();
  Shumway.FileLoadingService.instance = new Shumway.Player.ShumwayComFileLoadingService();
  Shumway.SystemResourcesLoadingService.instance = new Shumway.Player.ShumwayComResourcesLoadingService(true);
}

window.addEventListener('message', function onWindowMessage(e) {
  var data = e.data;
  if (typeof data !== 'object' || data === null) {
    return;
  }
  switch (data.type) {
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
