const path = require("path");
const webpack = require("webpack");

module.exports = {
  entry: [path.join(__dirname, "original.js")],
  output: {
    path: __dirname,
    filename: "bundle.js"
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
