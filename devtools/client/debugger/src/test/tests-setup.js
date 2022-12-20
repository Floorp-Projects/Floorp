/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

global.Worker = require("workerjs");

import path from "path";
import Enzyme from "enzyme";
import Adapter from "enzyme-adapter-react-16";
import { setupHelper } from "../utils/dbg";
import { prefs } from "../utils/prefs";

import {
  setJestWorkerURL as setJestPrettyPrintWorkerURL,
  stop as stopPrettyPrintWorker,
} from "../workers/pretty-print";

import { ParserDispatcher } from "../workers/parser";
import {
  setJestWorkerURL as setJestSearchWorkerURL,
  stop as stopSearchWorker,
} from "../workers/search";
import { clearDocuments } from "../utils/editor";

const rootPath = path.join(__dirname, "../../");

Enzyme.configure({ adapter: new Adapter() });

jest.setTimeout(20000);

function formatException(reason, p) {
  console && console.log("Unhandled Rejection at:", p, "reason:", reason);
}

export const parserWorker = new ParserDispatcher(
  path.join(rootPath, "src/workers/parser/worker.js")
);
export const evaluationsParser = new ParserDispatcher(
  path.join(rootPath, "src/workers/parser/worker.js")
);

beforeAll(() => {
  setJestPrettyPrintWorkerURL(
    path.join(rootPath, "src/workers/pretty-print/worker.js")
  );
  setJestSearchWorkerURL(path.join(rootPath, "src/workers/search/worker.js"));
  process.on("unhandledRejection", formatException);
});

afterAll(() => {
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
