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

var SHUMWAY_ROOT = "resource://shumway/";

var easel;
function createEasel(backgroundColor) {
  var Stage = Shumway.GFX.Stage;
  var Easel = Shumway.GFX.Easel;
  var Canvas2DRenderer = Shumway.GFX.Canvas2DRenderer;

  Shumway.GFX.WebGL.SHADER_ROOT = SHUMWAY_ROOT + "gfx/gl/shaders/";
  easel = new Easel(document.getElementById("easelContainer"), false, backgroundColor);
  
  if (ShumwayCom.environment === 'test') {
    ShumwayCom.setScreenShotCallback(function () {
      // flush rendering buffers
      easel.render();
      return easel.screenShot(null, true, false).dataURL;
    });
  }
  
  easel.startRendering();
  return easel;
}

var easelHost;
function createEaselHost() {
  var peer = new Shumway.Remoting.ShumwayComTransportPeer();
  easelHost = new Shumway.GFX.Window.WindowEaselHost(easel, peer);
  return easelHost;
}

function setHudVisible(visible) {
  Shumway.GFX.hud.value = !!visible;
}

function fallback() {
  parent.postMessage({callback: 'fallback'}, '*');
}

function showURL() {
  parent.postMessage({callback: 'showURL'}, '*' );
}

function showInInspector() {
  parent.postMessage({callback: 'showInInspector'}, '*');
}

function reportIssue() {
  parent.postMessage({callback: 'reportIssue'}, '*');
}

function showAbout() {
  parent.postMessage({callback: 'showAbout'}, '*');
}

function enableDebug() {
  parent.postMessage({callback: 'enableDebug'}, '*');
}

function prepareUI(params) {
  if (params.isOverlay) {
    var fallbackMenu = document.getElementById('fallbackMenu');
    fallbackMenu.removeAttribute('hidden');
    fallbackMenu.addEventListener('click', fallback);
  }
  document.getElementById('showURLMenu').addEventListener('click', showURL);
  document.getElementById('inspectorMenu').addEventListener('click', showInInspector);
  document.getElementById('reportMenu').addEventListener('click', reportIssue);
  document.getElementById('aboutMenu').addEventListener('click', showAbout);

  var version = Shumway.version || '';
  document.getElementById('aboutMenu').label =
    document.getElementById('aboutMenu').label.replace('%version%', version);

  if (params.isDebuggerEnabled) {
    document.getElementById('debugMenu').addEventListener('click', enableDebug);
  } else {
    document.getElementById('debugMenu').remove();
  }

  setHudVisible(params.isHudOn);

  createEasel(params.backgroundColor);
  createEaselHost();

  var displayParameters = easel.getDisplayParameters();
  window.parent.postMessage({
    callback: 'displayParameters',
    params: displayParameters
  }, '*');
}

window.addEventListener('message', function onWindowMessage(e) {
  var data = e.data;
  if (typeof data !== 'object' || data === null) {
    console.error('Unexpected message for gfx frame.');
    return;
  }
  switch (data.type) {
    case "prepareUI":
      prepareUI(data.params);
      break;
    default:
      console.error('Unexpected message for gfx frame: ' + args.callback);
      break;
  }
}, true);
