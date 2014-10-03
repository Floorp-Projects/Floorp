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
     * NOTE: It is recommended to use request() instead since one day we may not
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

window.print = function(msg) {
  console.log(msg);
};

var viewerPlayerglobalInfo = {
  abcs: SHUMWAY_ROOT + "playerglobal/playerglobal.abcs",
  catalog: SHUMWAY_ROOT + "playerglobal/playerglobal.json"
};

var builtinPath = SHUMWAY_ROOT + "avm2/generated/builtin/builtin.abc";
var avm1Path = SHUMWAY_ROOT + "avm2/generated/avm1lib/avm1lib.abc";

var playerWindow;
var playerWindowLoaded = new Promise(function(resolve) {
  var playerWindowIframe = document.getElementById("playerWindow");
  playerWindowIframe.addEventListener('load', function () {
    playerWindow = playerWindowIframe.contentWindow;
    resolve();
  });
  playerWindowIframe.src = 'resource://shumway/web/viewer.player.html';
});

function runViewer() {
  var flashParams = JSON.parse(FirefoxCom.requestSync('getPluginParams', null));

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

  playerWindowLoaded.then(function () {
    parseSwf(movieUrl, movieParams, objectParams);
  });

  if (isOverlay) {
    document.getElementById('overlay').className = 'enabled';
    var fallbackDiv = document.getElementById('fallback');
    fallbackDiv.addEventListener('click', function(e) {
      fallback();
      e.preventDefault();
    });
    var reportDiv = document.getElementById('report');
    reportDiv.addEventListener('click', function(e) {
      reportIssue();
      e.preventDefault();
    });
    var fallbackMenu = document.getElementById('fallbackMenu');
    fallbackMenu.removeAttribute('hidden');
    fallbackMenu.addEventListener('click', fallback);
  }
  document.getElementById('showURLMenu').addEventListener('click', showURL);
  document.getElementById('inspectorMenu').addEventListener('click', showInInspector);
  document.getElementById('reportMenu').addEventListener('click', reportIssue);
  document.getElementById('aboutMenu').addEventListener('click', showAbout);
}

function showURL() {
  window.prompt("Copy to clipboard", movieUrl);
}

function showInInspector() {
  var base = "http://www.areweflashyet.com/shumway/examples/inspector/inspector.html?rfile=";
  var params = '';
  for (var k in movieParams) {
    params += '&' + k + '=' + encodeURIComponent(movieParams[k]);
  }
  window.open(base + encodeURIComponent(movieUrl) + params);
}

function reportIssue() {
  //var duplicatesMap = Object.create(null);
  //var prunedExceptions = [];
  //avm2.exceptions.forEach(function(e) {
  //  var ident = e.source + e.message + e.stack;
  //  var entry = duplicatesMap[ident];
  //  if (!entry) {
  //    entry = duplicatesMap[ident] = {
  //      source: e.source,
  //      message: e.message,
  //      stack: e.stack,
  //      count: 0
  //    };
  //    prunedExceptions.push(entry);
  //  }
  //  entry.count++;
  //});
  //FirefoxCom.requestSync('reportIssue', JSON.stringify(prunedExceptions));
  FirefoxCom.requestSync('reportIssue');
}

function showAbout() {
  window.open('http://areweflashyet.com/');
}

var movieUrl, movieParams, objectParams;

window.addEventListener("message", function handlerMessage(e) {
  var args = e.data;
  switch (args.callback) {
    case 'loadFile':
      playerWindow.postMessage({
        type: "loadFileResponse",
        args: args
      }, '*');
      break;
    case 'loadFileRequest':
      FirefoxCom.request('loadFile', args.data, null);
      break;
    case 'reportTelemetry':
      FirefoxCom.request('reportTelemetry', args.data, null);
      break;
    case 'setClipboard':
      FirefoxCom.request('setClipboard', args.data, null);
      break;
    case 'started':
      document.body.classList.add('started');
      break;
  }
}, true);

var easelHost;

function processExternalCommand(command) {
  switch (command.action) {
    case 'isEnabled':
      command.result = true;
      break;
    case 'initJS':
      FirefoxCom.initJS(function (functionName, args) {
        return easelHost.sendExernalCallback(functionName, args);
      });
      break;
    default:
      command.result = FirefoxCom.requestSync('externalCom', command);
      break;
  }
}

function parseSwf(url, movieParams, objectParams) {
  var compilerSettings = JSON.parse(
    FirefoxCom.requestSync('getCompilerSettings', null));

  // init misc preferences
  var turboMode = FirefoxCom.requestSync('getBoolPref', {pref: 'shumway.turboMode', def: false});
  Shumway.GFX.backend.value = FirefoxCom.requestSync('getBoolPref', {pref: 'shumway.webgl', def: false}) ? 1 : 0;
  Shumway.GFX.hud.value = FirefoxCom.requestSync('getBoolPref', {pref: 'shumway.hud', def: false});
  //forceHidpi.value = FirefoxCom.requestSync('getBoolPref', {pref: 'shumway.force_hidpi', def: false});
  //dummyAnimation.value = FirefoxCom.requestSync('getBoolPref', {pref: 'shumway.dummyMode', def: false});

  console.info("Compiler settings: " + JSON.stringify(compilerSettings));
  console.info("Parsing " + url + "...");
  function loaded() {
    FirefoxCom.request('endActivation', null);
  }

  var bgcolor;
  if (objectParams) {
    var m;
    if (objectParams.bgcolor && (m = /#([0-9A-F]{6})/i.exec(objectParams.bgcolor))) {
      var hexColor = parseInt(m[1], 16);
      bgcolor = hexColor << 8 | 0xff;
    }
    if (objectParams.wmode === 'transparent') {
      bgcolor = 0;
    }
  }

  var easel = createEasel(bgcolor);
  easelHost = new Shumway.GFX.Window.WindowEaselHost(easel, playerWindow, window);
  easelHost.processExternalCommand = processExternalCommand;

  var data = {
    type: 'runSwf',
    settings: Shumway.Settings.getSettings(),
    flashParams: {
      compilerSettings: compilerSettings,
      movieParams: movieParams,
      objectParams: objectParams,
      turboMode: turboMode,
      bgcolor: bgcolor,
      url: url,
      baseUrl: url
    }
  };
  playerWindow.postMessage(data,  '*');
}

function createEasel(bgcolor) {
  var Stage = Shumway.GFX.Stage;
  var Easel = Shumway.GFX.Easel;
  var Canvas2DStageRenderer = Shumway.GFX.Canvas2DStageRenderer;

  Shumway.GFX.WebGL.SHADER_ROOT = SHUMWAY_ROOT + "gfx/gl/shaders/";
  var backend = Shumway.GFX.backend.value | 0;
  return new Easel(document.getElementById("stageContainer"), backend, false, bgcolor);
}
