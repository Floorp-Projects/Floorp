/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */

module.exports = {
  mode: "production",
  entry: {
    main: "./content/panels/js/main.js",
  },
  output: {
    filename: "[name].bundle.js",
    path: `${__dirname}/content/panels/js/`,
  },
  module: {
    rules: [
      {
        test: /\.jsx?$/,
        exclude: /node_modules/,
        loader: "babel-loader",
        options: {
          presets: ["@babel/preset-react"],
        },
      },
    ],
  },
  resolve: {
    extensions: [".js", ".jsx"],
  },
  optimization: {
    minimize: false,
    splitChunks: {
      cacheGroups: {
        vendor: {
          test: /[\\/]node_modules[\\/](react|react-dom|scheduler|object-assign)[\\/]/,
          name: "vendor",
          chunks: "all",
        },
      },
    },
  },
};
