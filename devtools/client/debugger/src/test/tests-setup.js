/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

global.Worker = require("workerjs");

import path from "path";
import Enzyme from "enzyme";
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

const rootPath = path.join(__dirname, "../../");

Enzyme.configure({ adapter: new Adapter() });

global.jasmine.DEFAULT_TIMEOUT_INTERVAL = 20000;

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
  clearDocuments();
  prefs.projectDirectoryRoot = "";
  prefs.projectDirectoryRootName = "";
  prefs.expressions = [];

  // Ensures window.dbg is there to track telemetry
  setupHelper({ selectors: {} });
});
