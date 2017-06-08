/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */
/* eslint max-len: [0] */

"use strict";

const {toolboxConfig} = require("./node_modules/devtools-launchpad/index");
const { NormalModuleReplacementPlugin } = require("webpack");
const {getConfig} = require("./bin/configure");

const path = require("path");
const projectPath = path.join(__dirname, "local-dev");

let webpackConfig = {
  entry: {
    console: [path.join(projectPath, "index.js")],
  },

  module: {
    loaders: [
      {
        test: /\.(png|svg)$/,
        loader: "file-loader?name=[path][name].[ext]",
      },
    ]
  },

  output: {
    path: path.join(__dirname, "assets/build"),
    filename: "[name].js",
    publicPath: "/assets/build",
  },

  externals: [
    {
      "promise": "var Promise",
    }
  ],
};

webpackConfig.resolve = {
  alias: {
    "Services": "devtools-modules/client/shared/shim/Services",

    "devtools/client/webconsole/jsterm": path.join(projectPath, "jsterm-stub"),
    "devtools/client/webconsole/utils": path.join(__dirname, "new-console-output/test/fixtures/WebConsoleUtils"),
    "devtools/client/webconsole/new-console-output": path.join(__dirname, "new-console-output"),
    "devtools/client/webconsole/webconsole-connection-proxy": path.join(__dirname, "webconsole-connection-proxy"),
    "devtools/client/webconsole/webconsole-l10n": path.join(__dirname, "webconsole-l10n"),

    "react": path.join(__dirname, "node_modules/react"),
    "devtools/client/shared/vendor/immutable": "immutable",
    "devtools/client/shared/vendor/react": "react",
    "devtools/client/shared/vendor/react-dom": "react-dom",
    "devtools/client/shared/vendor/react-redux": "react-redux",
    "devtools/client/shared/vendor/redux": "redux",

    "devtools/client/locales": path.join(__dirname, "../../client/locales/en-US"),
    "toolkit/locales": path.join(__dirname, "../../../toolkit/locales/en-US"),
    "devtools/shared/locales": path.join(__dirname, "../../shared/locales/en-US"),
    "devtools/shared/plural-form": path.join(__dirname, "../../shared/plural-form"),
    "devtools/shared/l10n": path.join(__dirname, "../../shared/l10n"),

    "devtools/client/framework/devtools": path.join(__dirname, "../../client/shims/devtools"),
    "devtools/client/framework/menu": "devtools-modules/client/framework/menu",
    "devtools/client/framework/menu-item": path.join(__dirname, "../../client/framework/menu-item"),

    "devtools/client/shared/components/reps/reps": path.join(__dirname, "../../client/shared/components/reps/reps"),
    "devtools/client/shared/redux/middleware/thunk": path.join(__dirname, "../../client/shared/redux/middleware/thunk"),
    "devtools/client/shared/components/stack-trace": path.join(__dirname, "../../client/shared/components/stack-trace"),
    "devtools/client/shared/source-utils": path.join(__dirname, "../../client/shared/source-utils"),
    "devtools/client/shared/components/frame": path.join(__dirname, "../../client/shared/components/frame"),

    "devtools/shared/defer": path.join(__dirname, "../../shared/defer"),
    "devtools/shared/event-emitter": "devtools-modules/shared/event-emitter",
    "devtools/shared/client/main": path.join(__dirname, "new-console-output/test/fixtures/ObjectClient"),
    "devtools/shared/platform/clipboard": path.join(__dirname, "../../shared/platform/content/clipboard"),
  }
};

const mappings = [
  [
    /utils\/menu/, "devtools-launchpad/src/components/shared/menu"
  ],
  [
    /chrome:\/\/devtools\/skin/,
    (result) => {
      result.request = result.request
        .replace("./chrome://devtools/skin", path.join(__dirname, "../themes"));
    }
  ],
  [
    /chrome:\/\/devtools\/content/,
    (result) => {
      result.request = result.request
        .replace("./chrome://devtools/content", path.join(__dirname, ".."));
    }
  ],
  [
    /resource:\/\/devtools/,
    (result) => {
      result.request = result.request
        .replace("./resource://devtools/client", path.join(__dirname, ".."));
    }
  ],
];

webpackConfig.plugins = mappings.map(([regex, res]) =>
  new NormalModuleReplacementPlugin(regex, res));

// Exclude to transpile all scripts in devtools/ but not for this folder
const basePath = path.join(__dirname, "../../").replace(/\\/g, "\\\\");
const baseName = path.basename(__dirname);
webpackConfig.babelExcludes = new RegExp(`^${basePath}(.(?!${baseName}))*$`);

let config = toolboxConfig(webpackConfig, getConfig());

// Remove loaders from devtools-launchpad's webpack.config.js
// * For svg-inline loader:
//   Webconsole uses file loader to bundle image assets instead of svg-inline loader
// * For raw loader:
//   devtools/shared/l10n has preloaded raw loader in require.context
config.module.loaders = config.module.loaders
  .filter((loader) => !["svg-inline", "raw"].includes(loader.loader));

module.exports = config;
