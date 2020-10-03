/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { readFileSync } from "fs";
import path from "path";

global.requestAnimationFrame = callback => setTimeout(callback, 0);
global.isWorker = false;

global.define = function() {};
global.loader = {
  lazyServiceGetter: () => {},
  lazyGetter: (context, name, fn) => {
    try {
      global[name] = fn();
    } catch (_) {}
  },
  lazyRequireGetter: (context, names, _path, destruct) => {
    if (
      !_path ||
      _path.startsWith("resource://") ||
      _path.match(/server\/actors/)
    ) {
      return;
    }

    const excluded = [
      "Debugger",
      "devtools/shared/event-emitter",
      "devtools/client/shared/autocomplete-popup",
      "devtools/client/framework/devtools",
      "devtools/client/shared/keycodes",
      "devtools/client/shared/sourceeditor/editor",
      "devtools/client/shared/telemetry",
      "devtools/shared/screenshot/save",
      "devtools/client/shared/focus",
    ];
    if (!excluded.includes(_path)) {
      if (!Array.isArray(names)) {
        names = [names];
      }

      for (const name of names) {
        // $FlowIgnore
        const module = require(_path);
        global[name] = destruct ? module[name] : module;
      }
    }
  },
};

global.DebuggerConfig = {};
const rootPath = path.join(__dirname, "../../");
function getL10nBundle() {
  const read = file => readFileSync(path.join(rootPath, file));

  try {
    return read("./assets/panel/debugger.properties");
  } catch (e) {
    return read("../locales/en-US/debugger.properties");
  }
}

global.L10N = require("devtools-launchpad").L10N;
global.L10N.setBundle(getL10nBundle());
global.performance = { now: () => 0 };

const { URL } = require("url");
global.URL = URL;

function mockIndexeddDB() {
  const store = {};
  return {
    open: () => ({}),
    getItem: async key => store[key],
    setItem: async (key, value) => {
      store[key] = value;
    },
  };
}
global.indexedDB = mockIndexeddDB();
