const path = require("path");
const webpack = require("webpack");

module.exports = {
  entry: {
    v1: "./v1.js",
    v2: "./v2.js",
    v3: "./v3.js"
  },
  output: {
    path: __dirname,
    filename: "[name].bundle.js"
  },
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
  ]
};
