/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const path = require("path");
const webpack = require("webpack");

const absolute = relPath => path.join(__dirname, relPath);

const resourcePathRegEx = /^resource:\/\/activity-stream\//;

module.exports = (env = {}) => ({
  mode: "none",
  entry: absolute("content-src/activity-stream.jsx"),
  output: {
    path: absolute("data/content"),
    filename: "activity-stream.bundle.js",
    library: "NewtabRenderUtils",
  },
  // TODO: switch to eval-source-map for faster builds. Requires CSP changes
  devtool: env.development ? "inline-source-map" : false,
  plugins: [
    new webpack.BannerPlugin(
      `THIS FILE IS AUTO-GENERATED: ${path.basename(__filename)}`
    ),
    new webpack.optimize.ModuleConcatenationPlugin(),
  ],
  module: {
    rules: [
      {
        test: /\.jsx?$/,
        exclude: /node_modules\/(?!(fluent|fluent-react)\/).*/,
        loader: "babel-loader",
        options: {
          presets: ["@babel/preset-react"],
        },
      },
      {
        test: /\.jsm$/,
        exclude: /node_modules/,
        loader: "babel-loader",
        // Converts .jsm files into common-js modules
        options: {
          plugins: [
            [
              "jsm-to-esmodules",
              {
                basePath: resourcePathRegEx,
                removeOtherImports: true,
                replace: true,
              },
            ],
          ],
        },
      },
    ],
  },
  // This resolve config allows us to import with paths relative to the root directory, e.g. "lib/ActivityStream.jsm"
  resolve: {
    extensions: [".js", ".jsx"],
    modules: ["node_modules", "."],
  },
  externals: {
    "prop-types": "PropTypes",
    react: "React",
    "react-dom": "ReactDOM",
    redux: "Redux",
    "react-redux": "ReactRedux",
    "react-transition-group": "ReactTransitionGroup",
  },
});
