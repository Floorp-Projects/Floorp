/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { toolboxConfig } = require("devtools-launchpad/index");
const config = require("./config");
const ObjectRestSpreadPlugin = require("@sucrase/webpack-object-rest-spread-plugin");

const path = require("path");
const projectPath = path.join(__dirname, "src");

const webpackConfig = {
  entry: {
    reps: [path.join(projectPath, "launchpad/index.js")],
  },

  output: {
    path: path.join(__dirname, "assets/build"),
    filename: "[name].js",
    publicPath: "/assets/build",
  },

  plugins: [new ObjectRestSpreadPlugin()],

  resolve: {
    alias: {
      "devtools/client/shared/vendor/react": "react",
      "devtools/client/shared/vendor/react-dom": "react-dom",
      "devtools/client/shared/vendor/react-dom-factories":
        "react-dom-factories",
      "devtools/client/shared/vendor/react-prop-types": "prop-types",
    },
  },
};

module.exports = toolboxConfig(webpackConfig, config);
