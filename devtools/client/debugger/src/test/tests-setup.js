/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

// $FlowIgnore
global.Worker = require("workerjs");

import path from "path";
import { readFileSync } from "fs";
import Enzyme from "enzyme";
// $FlowIgnore
import Adapter from "enzyme-adapter-react-16";
import { setupHelper } from "../utils/dbg";
import { prefs } from "../utils/prefs";

import { startSourceMapWorker, stopSourceMapWorker } from "devtools-source-map";

import {
  start as startPrettyPrintWorker,
  stop as stopPrettyPrintWorker,
} from "../workers/pretty-print";

import { ParserDispatcher } from "../workers/parser";
import {
  start as startSearchWorker,
  stop as stopSearchWorker,
} from "../workers/search";
import { clearDocuments } from "../utils/editor";
import { clearHistory } from "./utils/history";

import env from "devtools-environment/test-flag";
env.testing = true;

const rootPath = path.join(__dirname, "../../");

function getL10nBundle() {
  const read = file => readFileSync(path.join(rootPath, file));

  try {
    return read("./assets/panel/debugger.properties");
  } catch (e) {
    return read("../locales/en-US/debugger.properties");
  }
}

global.DebuggerConfig = {};
global.L10N = require("devtools-launchpad").L10N;
global.L10N.setBundle(getL10nBundle());
global.jasmine.DEFAULT_TIMEOUT_INTERVAL = 20000;
global.performance = { now: () => 0 };
global.isWorker = false;

global.define = function() {};
global.loader = {
  lazyServiceGetter: () => {},
  lazyGetter: (context, name, fn) => {
    try {
      global[name] = fn();
    } catch (_) {}
  },
  lazyRequireGetter: (context, name, _path, destruct) => {
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
      // $FlowIgnore
      const module = require(_path);
      global[name] = destruct ? module[name] : module;
    }
  },
};

const { URL } = require("url");
global.URL = URL;

global.indexedDB = mockIndexeddDB();

Enzyme.configure({ adapter: new Adapter() });

function formatException(reason, p) {
  console && console.log("Unhandled Rejection at:", p, "reason:", reason);
}

export const parserWorker = new ParserDispatcher();
export const evaluationsParser = new ParserDispatcher();

beforeAll(() => {
  startSourceMapWorker(
    path.join(rootPath, "node_modules/devtools-source-map/src/worker.js"),
    ""
  );
  startPrettyPrintWorker(
    path.join(rootPath, "src/workers/pretty-print/worker.js")
  );
  parserWorker.start(path.join(rootPath, "src/workers/parser/worker.js"));
  evaluationsParser.start(path.join(rootPath, "src/workers/parser/worker.js"));
  startSearchWorker(path.join(rootPath, "src/workers/search/worker.js"));
  process.on("unhandledRejection", formatException);
});

afterAll(() => {
  stopSourceMapWorker();
  stopPrettyPrintWorker();
  parserWorker.stop();
  evaluationsParser.stop();
  stopSearchWorker();
  process.removeListener("unhandledRejection", formatException);
});

afterEach(() => {});

beforeEach(async () => {
  parserWorker.clear();
  evaluationsParser.clear();
  clearHistory();
  clearDocuments();
  prefs.projectDirectoryRoot = "";
  prefs.expressions = [];

  // Ensures window.dbg is there to track telemetry
  setupHelper({ selectors: {} });
});

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

// NOTE: We polyfill finally because TRY uses node 8
if (!global.Promise.prototype.finally) {
  global.Promise.prototype.finally = function finallyPolyfill(callback) {
    const { constructor } = this;

    return this.then(
      function(value) {
        return constructor.resolve(callback()).then(function() {
          return value;
        });
      },
      function(reason) {
        return constructor.resolve(callback()).then(function() {
          throw reason;
        });
      }
    );
  };
}
