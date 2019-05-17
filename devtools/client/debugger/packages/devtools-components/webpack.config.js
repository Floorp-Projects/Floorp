/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { toolboxConfig } = require("devtools-launchpad/index");
const { isDevelopment } = require("devtools-config");

const path = require("path");
const projectPath = path.join(__dirname);

const webpackConfig = {
  entry: {
    "devtools-components": path.join(projectPath, "index.js"),
  },

  output: {
    path: path.join(__dirname, "assets/build"),
    filename: "[name].js",
    publicPath: "/assets/build",
    libraryTarget: "umd",
  },
  resolve: {
    alias: {
      "devtools/client/shared/vendor/react": "react",
      "devtools/client/shared/vendor/react-dom": "react-dom",
      "devtools/client/shared/vendor/react-dom-factories":
        "react-dom-factories",
      "devtools/client/shared/vendor/react-prop-types": "prop-types",
      Services: path.join(
        __dirname,
        "node_modules/devtools-modules/client/shared/shim/Services"
      ),
    },
  },
};

const extra = {
  disablePostCSS: true,
};
webpackConfig.plugins = [];
if (!isDevelopment()) {
  extra.excludeMap = {
    react: "devtools/client/shared/vendor/react",
    "react-dom": "devtools/client/shared/vendor/react-dom",
    "react-dom-factories": "devtools/client/shared/vendor/react-dom-factories",
    "prop-types": "devtools/client/shared/vendor/react-prop-types",
    lodash: "devtools/client/shared/vendor/lodash",
  };
}

module.exports = toolboxConfig(webpackConfig, {}, extra);
