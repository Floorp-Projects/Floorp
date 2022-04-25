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

const originalBundle = Object.assign({}, config, {
  entry: [path.join(__dirname, "original.js")],
  output: {
    path: __dirname,
    filename: "bundle.js",
  },
});

const bundleWithAnotherOriginalFile = Object.assign({}, config, {
  entry: [
    // This should cause the content of `original-with-no-update.js`
    // to shift in the new `bundle-with-another-original.js` generated.
    path.join(__dirname, "another-original.js"),
    path.join(__dirname, "original-with-no-update.js")
  ],
  output: {
    path: __dirname,
    filename: "bundle-with-another-original.js"
  }
});

const replacedBundle = Object.assign({}, config, {
  entry: [path.join(__dirname, "new-original.js")],
  output: {
    path: __dirname,
    filename: "replaced-bundle.js",
  },
});

module.exports = [originalBundle, bundleWithAnotherOriginalFile, replacedBundle];
