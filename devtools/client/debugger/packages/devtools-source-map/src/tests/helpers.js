/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { isOriginalId } = require("../utils");
const { getOriginalURLs } = require("../source-map");

const fs = require("fs");
const path = require("path");

function formatLocations(locs) {
  return locs.map(formatLocation).join(" -> ");
}

function formatLocation(loc) {
  const label = isOriginalId(loc.sourceId) ? "O" : "G";
  const col = loc.column === undefined ? "u" : loc.column;
  return `${label}[${loc.line}, ${col}]`;
}

function getMap(_path) {
  const mapPath = path.join(__dirname, _path);
  return fs.readFileSync(mapPath, "utf8");
}

async function setupBundleFixtureAndData(name) {
  const source = {
    id: `${name}.js`,
    sourceMapURL: `${name}.js.map`,
    sourceMapBaseURL: `http://example.com/${name}.js`,
  };

  require("devtools-utils/src/network-request").mockImplementationOnce(() => {
    const content = getMap(`fixtures/${name}.js.map`);
    return { content };
  });

  return getOriginalURLs(source);
}
async function setupBundleFixture(name) {
  const data = await setupBundleFixtureAndData(name);

  return data.map(item => item.url);
}

module.exports = {
  formatLocations,
  formatLocation,
  setupBundleFixture,
  setupBundleFixtureAndData,
  getMap,
};
