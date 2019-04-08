/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const path = require("path");
const util = require("util");
const _ = require("lodash");
const webpack = require("webpack");

const TARGET_NAME = "webpack3-babel6";

module.exports = exports = async function(tests, dirname) {
  const fixtures = [];
  for (const [name, input] of tests) {
    if (/typescript-/.test(name)) {
      continue;
    }

    const testFnName = _.camelCase(`${TARGET_NAME}-${name}`);
    const evalMaps = name.match(/-eval/);
    const babelEnv = !name.match(/-es6/);
    const babelModules = name.match(/-cjs/);

    console.log(`Building ${TARGET_NAME} test ${name}`);

    const scriptPath = path.join(dirname, "output", TARGET_NAME, `${name}.js`);
    const result = await util.promisify(webpack)({
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
            test: /\.js$/,
            exclude: /node_modules/,
            loader: require.resolve("babel-loader"),
            options: {
              babelrc: false,
              presets: [
                babelEnv
                  ? [
                      require.resolve("babel-preset-env"),
                      { modules: babelModules ? "commonjs" : false }
                    ]
                  : null
              ].filter(Boolean),
              plugins: [
                require.resolve("babel-plugin-transform-flow-strip-types"),
                require.resolve("babel-plugin-transform-class-properties")
              ]
            }
          }
        ].filter(Boolean)
      }
    });

    fixtures.push({
      name,
      testFnName: testFnName,
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
