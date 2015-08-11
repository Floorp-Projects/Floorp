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

window.print = function(msg) {
  console.log(msg);
};

function runSwfPlayer(flashParams, settings) {
  console.info('Time from init start to SWF player start: ' + (Date.now() - flashParams.initStartTime));
  if (settings) {
    Shumway.Settings.setSettings(settings);
  }
  setupServices();

  var asyncLoading = true;
  var baseUrl = flashParams.baseUrl;
  var objectParams = flashParams.objectParams;
  var movieUrl = flashParams.url;

  if (ShumwayCom.environment === 'test') {
    Shumway.frameRateOption.value = 60;
    Shumway.dontSkipFramesOption.value = true;
    
    window.print = function(msg) {
      ShumwayCom.print(msg.toString());
    };
    
    Shumway.Random.reset();
    Shumway.installTimeWarper();
  } else {
    Shumway.frameRateOption.value = flashParams.turboMode ? 60 : -1;
  }

  Shumway.createSecurityDomain(Shumway.AVM2LoadLibrariesFlags.Builtin | Shumway.AVM2LoadLibrariesFlags.Playerglobal).then(function (securityDomain) {
    function runSWF(file, buffer, baseUrl) {
      var peer = new Shumway.Remoting.ShumwayComTransportPeer();
      var gfxService = new Shumway.Player.Window.WindowGFXService(securityDomain, peer);
      var player = new Shumway.Player.Player(securityDomain, gfxService, flashParams.env);
      player.defaultStageColor = flashParams.bgcolor;
      player.movieParams = flashParams.movieParams;
      player.stageAlign = (objectParams && (objectParams.salign || objectParams.align)) || '';
      player.stageScale = (objectParams && objectParams.scale) || 'showall';
      player.displayParameters = flashParams.displayParameters;
      player.initStartTime = flashParams.initStartTime;

      player.pageUrl = baseUrl;
      console.info('Time from init start to SWF loading start: ' + (Date.now() - flashParams.initStartTime));
      player.load(file, buffer);
      playerStarted();
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
  Shumway.LocalConnectionService.instance = new Shumway.Player.ShumwayComLocalConnectionService();
}

function playerStarted() {
  document.body.style.backgroundColor = 'green';
  window.parent.postMessage({
    callback: 'started'
  }, '*');
}

window.addEventListener('message', function onWindowMessage(e) {
  var data = e.data;
  if (typeof data !== 'object' || data === null) {
    console.error('Unexpected message for player frame.');
    return;
  }
  switch (data.type) {
    case "runSwf":
      if (data.settings) {
        Shumway.Settings.setSettings(data.settings);
      }
      setupServices();
      runSwfPlayer(data.flashParams, data.settings);
      break;
    default:
      console.error('Unexpected message for player frame: ' + args.callback);
      break;
  }
}, true);
