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
    rules: [
      {
        test: /\.(png|svg)$/,
        loader: "file-loader?name=[path][name].[ext]",
      },
      {
        /*
         * The version of webpack used in the launchpad seems to have trouble
         * with the require("raw!${file}") that we use for the properties
         * file in l10.js.
         * This loader goes through the whole code and remove the "raw!" prefix
         * so the raw-loader declared in devtools-launchpad config can load
         * those files.
         */
        test: /\.js/,
        loader: "rewrite-raw",
      },
    ]
  },

  resolveLoader: {
    modules: [
      path.resolve("./node_modules"),
      path.resolve("./webpack"),
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
  modules: [
    // Make sure webpack is always looking for modules in
    // `webconsole/node_modules` directory first.
    path.resolve(__dirname, "node_modules"), "node_modules"
  ],
  alias: {
    "Services": "devtools-modules/src/Services",

    "devtools/client/webconsole/jsterm": path.join(projectPath, "jsterm-stub"),
    "devtools/client/webconsole/utils": path.join(__dirname, "new-console-output/test/fixtures/WebConsoleUtils"),
    "devtools/client/webconsole/new-console-output": path.join(__dirname, "new-console-output"),
    "devtools/client/webconsole/webconsole-connection-proxy": path.join(__dirname, "webconsole-connection-proxy"),
    "devtools/client/webconsole/webconsole-l10n": path.join(__dirname, "webconsole-l10n"),

    "devtools/client/shared/vendor/immutable": "immutable",
    "devtools/client/shared/vendor/react": "react",
    "devtools/client/shared/vendor/react-dom": "react-dom",
    "devtools/client/shared/vendor/react-redux": "react-redux",
    "devtools/client/shared/vendor/redux": "redux",
    "devtools/client/shared/vendor/reselect": "reselect",

    "devtools/client/locales": path.join(__dirname, "../../client/locales/en-US"),
    "toolkit/locales": path.join(__dirname, "../../../toolkit/locales/en-US"),
    "devtools/shared/locales": path.join(__dirname, "../../shared/locales/en-US"),
    "devtools/shim/locales": path.join(__dirname, "../../shared/locales/en-US"),
    "devtools/shared/plural-form": path.join(__dirname, "../../shared/plural-form"),
    "devtools/shared/l10n": path.join(__dirname, "../../shared/l10n"),
    "devtools/shared/system": path.join(projectPath, "system-stub"),

    "devtools/client/framework/devtools": path.join(__dirname, "../../client/shims/devtools"),
    "devtools/client/framework/menu": "devtools-modules/src/menu",
    "devtools/client/framework/menu-item": path.join(__dirname, "../../client/framework/menu-item"),
    "devtools/client/sourceeditor/editor": "devtools-source-editor/src/source-editor",

    "devtools/client/shared/redux/middleware/thunk": path.join(__dirname, "../../client/shared/redux/middleware/thunk"),
    "devtools/client/shared/redux/middleware/debounce": path.join(__dirname, "../../client/shared/redux/middleware/debounce"),

    "devtools/client/shared/components/reps/reps": path.join(__dirname, "../../client/shared/components/reps/reps"),
    "devtools/client/shared/components/stack-trace": path.join(__dirname, "../../client/shared/components/stack-trace"),
    "devtools/client/shared/components/search-box": path.join(__dirname, "../../client/shared/components/search-box"),
    "devtools/client/shared/components/splitter/draggable": path.join(__dirname, "../../client/shared/components/splitter/draggable"),
    "devtools/client/shared/components/splitter/split-box": path.join(__dirname, "../../client/shared/components/splitter/split-box"),
    "devtools/client/shared/components/frame": path.join(__dirname, "../../client/shared/components/frame"),
    "devtools/client/shared/components/autocomplete-popup": path.join(__dirname, "../../client/shared/components/autocomplete-popup"),
    "devtools/client/shared/components/tabs/tabbar": path.join(__dirname, "../../client/shared/components/tabs/tabbar"),
    "devtools/client/shared/components/tabs/tabs": path.join(__dirname, "../../client/shared/components/tabs/tabs"),
    "devtools/client/shared/components/tree/tree-view": path.join(__dirname, "../../client/shared/components/tree/tree-view"),
    "devtools/client/shared/components/tree/tree-row": path.join(__dirname, "../../client/shared/components/tree/tree-row"),

    "devtools/client/shared/source-utils": path.join(__dirname, "../../client/shared/source-utils"),
    "devtools/client/shared/key-shortcuts": path.join(__dirname, "../../client/shared/key-shortcuts"),
    "devtools/client/shared/keycodes": path.join(__dirname, "../../client/shared/keycodes"),
    "devtools/client/shared/zoom-keys": "devtools-modules/src/zoom-keys",
    "devtools/client/shared/curl": path.join(__dirname, "../../client/shared/curl"),
    "devtools/client/shared/scroll": path.join(__dirname, "../../client/shared/scroll"),

    "devtools/shared/fronts/timeline": path.join(__dirname, "../../shared/shims/fronts/timeline"),
    "devtools/shared/defer": path.join(__dirname, "../../shared/defer"),
    "devtools/shared/old-event-emitter": "devtools-modules/src/utils/event-emitter",
    "devtools/shared/client/main": path.join(__dirname, "new-console-output/test/fixtures/ObjectClient"),
    "devtools/shared/platform/clipboard": path.join(__dirname, "../../shared/platform/content/clipboard"),
    "devtools/shared/platform/stack": path.join(__dirname, "../../shared/platform/content/stack"),

    "devtools/client/netmonitor/src/utils/request-utils": path.join(__dirname, "../netmonitor/src/utils/request-utils"),
    "devtools/client/netmonitor/src/components/tabbox-panel": path.join(__dirname, "../netmonitor/src/components/tabbox-panel"),
    "devtools/client/netmonitor/src/connector/firefox-data-provider": path.join(__dirname, "../netmonitor/src/connector/firefox-data-provider"),
    "devtools/client/netmonitor/src/constants": path.join(__dirname, "../netmonitor/src/constants"),
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

const basePath = path.join(__dirname, "../../").replace(/\\/g, "\\\\");
const baseName = path.basename(__dirname);

let config = toolboxConfig(webpackConfig, getConfig(), {
  // Exclude to transpile all scripts in devtools/ but not for this folder
  babelExcludes: new RegExp(`^${basePath}(.(?!${baseName}))*$`)
});

// Remove loaders from devtools-launchpad's webpack.config.js
// * For svg-inline loader:
//   Webconsole uses file loader to bundle image assets instead of svg-inline-loader
config.module.rules = config.module.rules
  .filter((rule) => !["svg-inline-loader"].includes(rule.loader));

module.exports = config;
