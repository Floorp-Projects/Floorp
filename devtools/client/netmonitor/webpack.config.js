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

const webpackConfig = {
  entry: {
    netmonitor: [path.join(__dirname, "launchpad.js")]
  },

  module: {
    rules: [
      {
        test: /\.(png|svg)$/,
        loader: "file-loader?name=[path][name].[ext]",
      },
      {
        test: /\.js$/,
        loaders: [
          /**
            * The version of webpack used in the launchpad seems to have trouble
            * with the require("raw!${file}") that we use for the properties
            * file in l10.js.
            * This loader goes through the whole code and remove the "raw!" prefix
            * so the raw-loader declared in devtools-launchpad config can load
            * those files.
            */
          "rewrite-raw",
          // Replace all references to this.browserRequire() by require()
          "rewrite-browser-require",
          // Replace all references to loader.lazyRequire() by require()
          "rewrite-lazy-require",
          // Replace all references to loader.lazyGetter() by require()
          "rewrite-lazy-getter",
        ],
      }
    ]
  },

  resolveLoader: {
    modules: [
      "node_modules",
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
      // `netmonitor/node_modules` directory first.
      path.resolve(__dirname, "node_modules"),
      "node_modules",
    ],
    alias: {
      "Services": "devtools-modules/src/Services",
      "react": path.join(__dirname, "node_modules/react"),

      "devtools/client/framework/devtools": path.join(__dirname, "../../client/shared/webpack/shims/framework-devtools-shim"),
      "devtools/client/framework/menu": "devtools-modules/src/menu",
      "devtools/client/netmonitor/src/utils/menu": "devtools-contextmenu",

      "devtools/client/shared/vendor/react": "react",
      "devtools/client/shared/vendor/react-dom": "react-dom",
      "devtools/client/shared/vendor/react-redux": "react-redux",
      "devtools/client/shared/vendor/redux": "redux",
      "devtools/client/shared/vendor/reselect": "reselect",
      "devtools/client/shared/vendor/jszip": "jszip",

      "devtools/client/sourceeditor/editor": "devtools-source-editor/src/source-editor",

      "devtools/shared/event-emitter": "devtools-modules/src/utils/event-emitter",
      "devtools/shared/fronts/timeline": path.join(__dirname, "../../client/shared/webpack/shims/fronts-timeline-shim"),
      "devtools/shared/platform/clipboard": path.join(__dirname, "../../client/shared/webpack/shims/platform-clipboard-stub"),
      "devtools/client/netmonitor/src/utils/firefox/open-request-in-tab": path.join(__dirname, "src/utils/open-request-in-tab"),
      "devtools/client/shared/unicode-url": "./node_modules/devtools-modules/src/unicode-url",

      // Locales need to be explicitly mapped to the en-US subfolder
      "devtools/client/locales": path.join(__dirname, "../../client/locales/en-US"),
      "devtools/shared/locales": path.join(__dirname, "../../shared/locales/en-US"),
      "devtools/startup/locales": path.join(__dirname, "../../shared/locales/en-US"),
      "toolkit/locales": path.join(__dirname, "../../../toolkit/locales/en-US"),

      // Unless a path explicitly needs to be rewritten or shimmed, all devtools paths can
      // be mapped to ../../
      "devtools": path.join(__dirname, "../../"),
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

const config = toolboxConfig(webpackConfig, getConfig(), {
  // Exclude to transpile all scripts in devtools/ but not for this folder
  babelExcludes: new RegExp(`^${basePath}(.(?!${baseName}))*$`)
});

// Remove loaders from devtools-launchpad's webpack.config.js
// For svg-inline loader:
// Using file loader to bundle image assets instead of svg-inline-loader
config.module.rules = config.module.rules.filter((rule) => !["svg-inline-loader"].includes(rule.loader));

// For PostCSS loader:
// Disable PostCSS loader
config.module.rules.forEach(rule => {
  if (Array.isArray(rule.use)) {
    rule.use.some((use, idx) => {
      if (use.loader === "postcss-loader") {
        rule.use = rule.use.slice(0, idx);
        return true;
      }
      return false;
    });
  }
});

module.exports = config;
