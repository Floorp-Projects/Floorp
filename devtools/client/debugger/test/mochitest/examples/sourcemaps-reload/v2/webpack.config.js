const path = require("path");
const webpack = require("webpack");

const config = {
  devtool: "sourcemap",
  module: {
    loaders: [
      {
        test: /\.js$/,
        exclude: /node_modules/,
        loader: "babel-loader"
      }
    ]
  },
  plugins: [
  ],
};

const originalBundle = Object.assign({}, config, {
  entry: [
    path.join(__dirname, "original.js"),
  ],
  output: {
    path: __dirname,
    filename: "bundle.js"
  },
});

const replacedBundle = Object.assign({}, config, {
  entry: [
    path.join(__dirname, "new-original.js"),
  ],
  output: {
    path: __dirname,
    filename: "replaced-bundle.js"
  },
});

module.exports = [originalBundle, replacedBundle];
