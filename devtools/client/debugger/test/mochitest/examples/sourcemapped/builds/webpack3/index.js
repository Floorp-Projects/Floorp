/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const path = require("path");
const util = require("util");
const _ = require("lodash");
const webpack = require("webpack");

const TARGET_NAME = "webpack3";

module.exports = exports = async function(tests, dirname) {
  const fixtures = [];
  for (const [name, input] of tests) {
    if (/babel-/.test(name)) {
      continue;
    }

    const testFnName = _.camelCase(`${TARGET_NAME}-${name}`);
    const evalMaps = name.match(/-eval/);

    console.log(`Building ${TARGET_NAME} test ${name}`);

    const scriptPath = path.join(dirname, "output", TARGET_NAME, `${name}.js`);
    await util.promisify(webpack)({
      context: path.dirname(input),
      entry: `./${path.basename(input)}`,
      output: {
        path: path.dirname(scriptPath),
        filename: path.basename(scriptPath),

        devtoolModuleFilenameTemplate: `${TARGET_NAME}://./${name}/[resource-path]`,

        libraryTarget: "var",
        library: testFnName,
        libraryExport: "default"
      },
      devtool: evalMaps ? "eval-source-map" : "source-map",
      module: {
        loaders: [
          {
            test: /\.tsx?$/,
            exclude: /node_modules/,
            loader: require.resolve("ts-loader"),
            options: {}
          }
        ].filter(Boolean)
      }
    });

    fixtures.push({
      name,
      testFnName,
      scriptPath,
      assets: [scriptPath, evalMaps ? null : `${scriptPath}.map`].filter(
        Boolean
      )
    });
  }

  return {
    target: TARGET_NAME,
    fixtures
  };
};
