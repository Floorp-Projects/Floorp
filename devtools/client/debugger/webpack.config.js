/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const CopyWebpackPlugin = require("copy-webpack-plugin");
const webpack = require("webpack");
const ExtractTextPlugin = require("extract-text-webpack-plugin");

const mozillaCentralMappings = require("./configs/mozilla-central-mappings");
const path = require("path");
const ObjectRestSpreadPlugin = require("@sucrase/webpack-object-rest-spread-plugin");

/*
 * builds a path that's relative to the project path
 * returns an array so that we can prepend
 * hot-module-reloading in local development
 */
function getEntry(filename) {
  return [path.join(__dirname, filename)];
}

module.exports = {
  context: path.resolve(__dirname, "src"),
  devtool: false,
  node: { fs: "empty" },
  recordsPath: path.join(__dirname, "bin/module-manifest.json"),
  entry: {
    vendors: getEntry("src/vendors.js"),
  },

  output: {
    path: process.env.OUTPUT_PATH,
    filename: "[name].js",
    publicPath: "/assets/build",
    libraryTarget: "umd",
  },

  plugins: [
    new webpack.BannerPlugin({
      banner: `/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
`,
      raw: true,
    }),
    new ObjectRestSpreadPlugin(),
    new ExtractTextPlugin("[name].css"),
    new webpack.DefinePlugin({
      "process.env": {
        NODE_ENV: JSON.stringify(process.env.NODE_ENV || "production"),
      },
    }),
  ],
  module: {
    rules: [
      {
        test: /\.json$/,
        loader: "json-loader",
      },
      {
        test: /\.js$/,
        exclude: request => {
          // Some paths are excluded from Babel
          const excludedPaths = ["fs", "node_modules"];
          const excludedRe = new RegExp(`(${excludedPaths.join("|")})`);
          const excluded = !!request.match(excludedRe);
          const included = ["devtools-", "react-aria-components"];

          const reincludeRe = new RegExp(
            `node_modules(\\/|\\\\)${included.join("|")}`
          );
          return excluded && !request.match(reincludeRe);
        },
        loader: require.resolve("babel-loader"),
        options: {
          ignore: ["src/lib"],
        },
      },
      {
        test: /\.properties$/,
        loader: "raw-loader",
      },
      // Extract CSS into a single file
      {
        test: /\.css$/,
        use: ExtractTextPlugin.extract({
          filename: "*.css",
          use: [
            {
              loader: "css-loader",
              options: {
                importLoaders: 1,
                url: false,
              },
            },
          ],
        }),
      },
    ],
  },
  externals: [
    function externalsTest(context, mod, callback) {
      // Any matching paths here won't be included in the bundle.
      if (mozillaCentralMappings[mod]) {
        callback(null, mozillaCentralMappings[mod]);
        return;
      }

      callback();
    },
  ],
};
