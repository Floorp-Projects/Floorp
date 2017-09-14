/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */
/* eslint max-len: [0] */

"use strict";

const path = require("path");
const { NormalModuleReplacementPlugin } = require("webpack");
const { toolboxConfig } = require("./node_modules/devtools-launchpad/index");
const { getConfig } = require("./bin/configure");

let webpackConfig = {
  entry: {
    netmonitor: [path.join(__dirname, "index.js")]
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
        test: /\.js$/,
        loader: "rewrite-raw",
      },
      {
        test: /\.js$/,
        loaders: [
          // Replace all references to this.browserRequire() by require()
          "rewrite-browser-require",
          // Replace all references to loader.lazyRequire() by require()
          "rewrite-lazy-require",
        ],
      }
    ]
  },

  resolveLoader: {
    modules: [
      path.resolve("./node_modules"),
      path.resolve("../shared/webpack"),
    ]
  },

  output: {
    path: path.join(__dirname, "assets/build"),
    filename: "[name].js",
    publicPath: "/assets/build",
    libraryTarget: "umd",
  },

  resolve: {
    modules: [
      // Make sure webpack is always looking for modules in
      // `webconsole/node_modules` directory first.
      path.resolve(__dirname, "node_modules"), "node_modules"
    ],
    alias: {
      "Services": "devtools-modules/src/Services",
      "react": path.join(__dirname, "node_modules/react"),

      "devtools/client/framework/devtools": path.join(__dirname, "../../client/shims/devtools"),
      "devtools/client/framework/menu": "devtools-modules/src/menu",
      "devtools/client/framework/menu-item": path.join(__dirname, "../../client/framework/menu-item"),
      "devtools/client/locales": path.join(__dirname, "../../client/locales/en-US"),
      "devtools/client/netmonitor/src/utils/menu": "devtools-contextmenu",

      "devtools/client/shared/components/autocomplete-popup": path.join(__dirname, "../../client/shared/components/autocomplete-popup"),
      "devtools/client/shared/components/reps/reps": path.join(__dirname, "../../client/shared/components/reps/reps"),
      "devtools/client/shared/components/search-box": path.join(__dirname, "../../client/shared/components/search-box"),
      "devtools/client/shared/components/splitter/draggable": path.join(__dirname, "../../client/shared/components/splitter/draggable"),
      "devtools/client/shared/components/splitter/split-box": path.join(__dirname, "../../client/shared/components/splitter/split-box"),
      "devtools/client/shared/components/stack-trace": path.join(__dirname, "../../client/shared/components/stack-trace"),
      "devtools/client/shared/components/tabs/tabbar": path.join(__dirname, "../../client/shared/components/tabs/tabbar"),
      "devtools/client/shared/components/tabs/tabs": path.join(__dirname, "../../client/shared/components/tabs/tabs"),
      "devtools/client/shared/components/tree/tree-view": path.join(__dirname, "../../client/shared/components/tree/tree-view"),
      "devtools/client/shared/components/tree/tree-row": path.join(__dirname, "../../client/shared/components/tree/tree-row"),
      "devtools/client/shared/curl": path.join(__dirname, "../../client/shared/curl"),
      "devtools/client/shared/file-saver": path.join(__dirname, "../../client/shared/file-saver"),
      "devtools/client/shared/keycodes": path.join(__dirname, "../../client/shared/keycodes"),
      "devtools/client/shared/key-shortcuts": path.join(__dirname, "../../client/shared/key-shortcuts"),
      "devtools/client/shared/prefs": path.join(__dirname, "../../client/shared/prefs"),
      "devtools/client/shared/scroll": path.join(__dirname, "../../client/shared/scroll"),
      "devtools/client/shared/source-utils": path.join(__dirname, "../../client/shared/source-utils"),
      "devtools/client/shared/theme": path.join(__dirname, "../../client/shared/theme"),
      "devtools/client/shared/vendor/immutable": "immutable",
      "devtools/client/shared/vendor/react": "react",
      "devtools/client/shared/vendor/react-dom": "react-dom",
      "devtools/client/shared/vendor/react-redux": "react-redux",
      "devtools/client/shared/vendor/redux": "redux",
      "devtools/client/shared/vendor/reselect": "reselect",
      "devtools/client/shared/vendor/jszip": "jszip",
      "devtools/client/shared/widgets/tooltip/HTMLTooltip": path.join(__dirname, "../../client/shared/widgets/tooltip/HTMLTooltip"),
      "devtools/client/shared/widgets/tooltip/ImageTooltipHelper": path.join(__dirname, "../../client/shared/widgets/tooltip/ImageTooltipHelper"),
      "devtools/client/shared/widgets/tooltip/TooltipToggle": path.join(__dirname, "../../client/shared/widgets/tooltip/TooltipToggle"),
      "devtools/client/shared/widgets/Chart": path.join(__dirname, "../../client/shared/widgets/Chart"),

      "devtools/client/sourceeditor/editor": "devtools-source-editor/src/source-editor",
      "devtools/client/themes/variables.css": path.join(__dirname, "../../client/themes/variables.css"),

      "devtools/shared/async-utils": path.join(__dirname, "../../shared/async-utils"),
      "devtools/shared/defer": path.join(__dirname, "../../shared/defer"),
      "devtools/shared/old-event-emitter": "devtools-modules/src/utils/event-emitter",
      "devtools/shared/fronts/timeline": path.join(__dirname, "../../shared/shims/fronts/timeline"),
      "devtools/shared/l10n": path.join(__dirname, "../../shared/l10n"),
      "devtools/shared/locales": path.join(__dirname, "../../shared/locales/en-US"),
      "devtools/shared/platform/clipboard": path.join(__dirname, "../../shared/platform/content/clipboard"),
      "devtools/shared/plural-form": path.join(__dirname, "../../shared/plural-form"),
      "devtools/shared/css/color": path.join(__dirname, "../../shared/css/color"),
      "devtools/shared/css/color-db": path.join(__dirname, "../../shared/css/color-db"),
      "devtools/shared/css/lexer": path.join(__dirname, "../../shared/css/lexer"),
      "devtools/shared/css/parsing-utils": path.join(__dirname, "../../shared/css/parsing-utils"),
      "devtools/shared/css/properties-db": path.join(__dirname, "../../shared/css/properties-db"),
      "devtools/shared/css/generated/properties-db": path.join(__dirname, "../../shared/css/generated/properties-db"),
      "devtools/shared/task": path.join(__dirname, "../../shared/task"),

      "devtools/shim/locales": path.join(__dirname, "../../shared/locales/en-US"),
      "toolkit/locales": path.join(__dirname, "../../../toolkit/locales/en-US"),
    },
  },
};

const mappings = [
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
//   Netmonitor uses file loader to bundle image assets instead of svg-inline-loader
config.module.rules = config.module.rules
  .filter((rule) => !["svg-inline-loader"].includes(rule.loader));

module.exports = config;
