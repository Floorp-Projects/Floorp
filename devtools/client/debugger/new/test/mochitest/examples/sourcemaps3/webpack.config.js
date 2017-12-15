const path = require("path");
const webpack = require("webpack");

module.exports = {
  entry: "./test.js",
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
    new webpack.optimize.UglifyJsPlugin({
      compress: {
        warnings: false
      },
      sourceMap: true,
      output: {
        comments: false
      }
    })
  ]
};
