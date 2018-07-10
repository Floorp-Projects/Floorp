"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.setupHelper = setupHelper;

var _timings = require("./timings");

var timings = _interopRequireWildcard(_timings);

var _prefs = require("./prefs");

var _devtoolsEnvironment = require("devtools/client/debugger/new/dist/vendors").vendored["devtools-environment"];

var _pausePoints = require("./pause/pausePoints");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function findSource(dbg, url) {
  const sources = dbg.selectors.getSources();
  const source = sources.find(s => (s.url || "").includes(url));

  if (!source) {
    return;
  }

  return source;
}

function sendPacket(dbg, packet, callback) {
  dbg.client.sendPacket(packet, callback || console.log);
}

function sendPacketToThread(dbg, packet, callback) {
  sendPacket(dbg, {
    to: dbg.connection.tabConnection.threadClient.actor,
    ...packet
  }, callback);
}

function evaluate(dbg, expression, callback) {
  dbg.client.evaluate(expression).then(callback || console.log);
}

function bindSelectors(obj) {
  return Object.keys(obj.selectors).reduce((bound, selector) => {
    bound[selector] = (a, b, c) => obj.selectors[selector](obj.store.getState(), a, b, c);

    return bound;
  }, {});
}

function getCM() {
  const cm = document.querySelector(".CodeMirror");
  return cm && cm.CodeMirror;
}

function _formatPausePoints(dbg, url) {
  const source = dbg.helpers.findSource(url);
  const pausePoints = dbg.selectors.getPausePoints(source);
  console.log((0, _pausePoints.formatPausePoints)(source.text, pausePoints));
}

function setupHelper(obj) {
  const selectors = bindSelectors(obj);
  const dbg = { ...obj,
    selectors,
    prefs: _prefs.prefs,
    features: _prefs.features,
    timings,
    getCM,
    helpers: {
      findSource: url => findSource(dbg, url),
      evaluate: (expression, cbk) => evaluate(dbg, expression, cbk),
      sendPacketToThread: (packet, cbk) => sendPacketToThread(dbg, packet, cbk),
      sendPacket: (packet, cbk) => sendPacket(dbg, packet, cbk)
    },
    formatters: {
      pausePoints: url => _formatPausePoints(dbg, url)
    }
  };
  window.dbg = dbg;

  if ((0, _devtoolsEnvironment.isDevelopment)()) {
    console.group("Development Notes");
    const baseUrl = "https://devtools-html.github.io/debugger.html";
    const localDevelopmentUrl = `${baseUrl}/docs/dbg.html`;
    console.log("Debugging Tips", localDevelopmentUrl);
    console.log("dbg", window.dbg);
    console.groupEnd();
  }
}