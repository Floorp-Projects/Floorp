const path = require("path");
const webpack = require("webpack");

const config = {
  devtool: "sourcemap",
};

if (webpack.version && webpack.version[0] === "4") {
  // Webpack 3, used in sourcemaps-reload-uncompressed, doesn't support mode attribute.
  // Webpack 4, used in sourcemaps-reload-compressed we want production mode in order to compress the sources
  config.mode = "production";
} else {
  // Also Webpack 4 doesn't support the module.loaders attribute
  config.module = {
    loaders: [
      {
        test: /\.js$/,
        exclude: /node_modules/,
        loader: "babel-loader",
      },
    ],
  };
}

module.exports = Object.assign({}, config, {
  entry: [path.join(__dirname, "original.js")],
  output: {
    path: __dirname,
    filename: "bundle.js",
  },
});
