/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global __dirname */

"use strict";

const {toolboxConfig} = require("devtools-launchpad/index");

const path = require("path");
const webpack = require("webpack");

module.exports = envConfig => {
  let webpackConfig = {
    bail: false,
    entry: [
      path.join(__dirname, "local-toolbox.js")
    ],
    output: {
      path: path.join(__dirname, "assets/build"),
      filename: "inspector.js",
      publicPath: "/"
    },
    module: {
      // Disable handling of unknown requires
      unknownContextRegExp: /$^/,
      unknownContextCritical: false,

      // Disable handling of requires with a single expression
      exprContextRegExp: /$^/,
      exprContextCritical: false,

      // Warn for every expression in require
      wrappedContextCritical: true,

      loaders: [
        {
          test: /event-emitter/,
          exclude: /node_modules/,
          loaders: [path.join(__dirname, "./webpack/rewrite-event-emitter")],
        }, {
          // Replace all references to this.browserRequire() by require() in
          // client/inspector/*.js files
          test: /client(\/|\\)inspector(\/|\\).*\.js$/,
          loaders: [path.join(__dirname, "./webpack/rewrite-browser-require")],
        }
      ]
    },
    resolveLoader: {
      root: [
        path.resolve("./node_modules"),
        path.resolve("./webpack"),
      ]
    },
    resolve: {
      alias: {
        "acorn/util/walk": path.join(__dirname, "../../shared/acorn/walk"),
        "acorn": path.join(__dirname, "../../shared/acorn"),
        "devtools/client/framework/about-devtools-toolbox":
          path.join(__dirname, "./webpack/about-devtools-sham.js"),
        "devtools/client/framework/attach-thread":
          path.join(__dirname, "./webpack/attach-thread-sham.js"),
        "devtools/client/framework/target-from-url":
          path.join(__dirname, "./webpack/target-from-url-sham.js"),
        "devtools/client/jsonview/main":
          path.join(__dirname, "./webpack/jsonview-sham.js"),
        "devtools/client/sourceeditor/editor":
          path.join(__dirname, "./webpack/editor-sham.js"),
        "devtools/client/locales": path.join(__dirname, "../locales/en-US"),
        "devtools/shared/DevToolsUtils":
          path.join(__dirname, "./webpack/devtools-utils-sham.js"),
        "devtools/shared/locales": path.join(__dirname, "../../shared/locales/en-US"),
        "devtools/shared/platform": path.join(__dirname, "../../shared/platform/content"),
        "devtools": path.join(__dirname, "../../"),
        "gcli": path.join(__dirname, "../../shared/gcli/source/lib/gcli"),
        "method": path.join(__dirname, "../../../addon-sdk/source/lib/method"),
        "modules/libpref/init/all":
          path.join(__dirname, "../../../modules/libpref/init/all.js"),
        "sdk/system/unload": path.join(__dirname, "./webpack/system-unload-sham.js"),
        "sdk": path.join(__dirname, "../../../addon-sdk/source/lib/sdk"),
        "Services": path.join(__dirname, "../shared/shim/Services.js"),
        "toolkit/locales":
          path.join(__dirname, "../../../toolkit/locales/en-US/chrome/global"),
        "react": "devtools/client/shared/vendor/react",
        "react-dom": "devtools/client/shared/vendor/react-dom",
      },
    },

    plugins: [
      new webpack.DefinePlugin({
        "isWorker": JSON.stringify(false),
        "reportError": "console.error",
        "AppConstants": "{ DEBUG: true, DEBUG_JS_MODULES: true }",
        "loader": `{
                      lazyRequireGetter: () => {},
                      lazyGetter: () => {}
                    }`,
        "dump": "console.log",
      })
    ]
  };

  webpackConfig.externals = [
    /codemirror\//,
    {
      "promise": "var Promise",
      "devtools/server/main": "{}",

      // Just trying to get build to work.  These should be removed eventually:
      "chrome": "{}",

      // In case you end up in chrome-land you can use this to help track down issues.
      // SDK for instance does a bunch of this so if you somehow end up importing an SDK
      // dependency this might help for debugging:
      // "chrome": `{
      //   Cc: {
      //     "@mozilla.org/uuid-generator;1": { getService: () => { return {} } },
      //     "@mozilla.org/observer-service;1": { getService: () => { return {} } },
      //   },
      //   Ci: {},
      //   Cr: {},
      //   Cm: {},
      //   components: { classesByID: () => {} , ID: () => {} }
      // }`,

      "resource://gre/modules/XPCOMUtils.jsm": "{}",
      "resource://devtools/client/styleeditor/StyleEditorUI.jsm": "{}",
      "resource://devtools/client/styleeditor/StyleEditorUtil.jsm": "{}",
      "devtools/client/shared/developer-toolbar": "{}",
    },
  ];

  // Exclude all files from devtools/ or addon-sdk/ or modules/ .
  webpackConfig.babelExcludes = /(devtools(\/|\\)|addon-sdk(\/|\\)|modules(\/|\\))/;

  return toolboxConfig(webpackConfig, envConfig);
};
