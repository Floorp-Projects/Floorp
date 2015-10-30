/* eslint-env node */

// Webpack hurts to debug (lots of trial-and-error), so put non-trivial
// functionality in other script logic whenever practical.

var path = require("path");
var webpack = require("webpack");

function getSharedDir() {
  "use strict";

  var fs = require("fs");

  // relative path to the shared directory in m-c
  var mozillaCentralShared =
    path.resolve(path.join(__dirname, "..", "content", "shared"));

  try {
    // if this doesn't blow up...
    fs.statSync(mozillaCentralShared);

    // that directory is there, so return it
    return mozillaCentralShared;

  } catch (ex) {
    // otherwise, assume that we're in loop-client, where the shared
    // directory is in the content directory
    return path.join(__dirname, "content", "shared");
  }
}

/**
 * See http://webpack.github.io/docs/configuration.html on how this works.
 * Make generous use of a search engine and stack overflow to interpret
 * those docs.
 */
module.exports = {
  entry: "./content/webappEntryPoint.js",

  // These symbols come in either via <script> tags or the expose loader,
  // so we tell webpack to allow them to be unresolved
  externals: {
    "Backbone": "Backbone",
    "navigator.mozL10n": "navigator.mozL10n",
    "React": "React",
    "underscore": "_"
  },

  // We want the shared modules to be available without the requirer needing
  // to know the path to them, especially since that path is different in
  // mozilla-central and loop-client.
  resolve: {
    alias: {
      shared: getSharedDir()
    }
  },

  output: {
    filename: "standalone.js",
    path: path.join(__dirname, "dist", "js"),
    // Reduce false-positive warnings when viewing in editors which read
    // our eslint config files (which disallows tabs).
    sourcePrefix: "  "
  },

  plugins: [
    // This is a soft-dependency for Backbone, and since we don't use it,
    // we need to tell webpack to ignore the unresolved reference. Cribbed
    // from
    // https://github.com/jashkenas/backbone/wiki/Using-Backbone-without-jQuery
    new webpack.IgnorePlugin(/^jquery$/),

    // definePlugin takes raw strings and inserts them, so we can put a
    // JS string for evaluation at build time.  Right now, we test NODE_ENV
    // to so we can do different things at build time based on whether we're
    // in production mode.
    new webpack.DefinePlugin({
      __PROD__: JSON.stringify(JSON.parse(
        process.env.NODE_ENV && process.env.NODE_ENV === "production"))
    }),

    new webpack.optimize.UglifyJsPlugin({
      compress: {
        // XXX I'd _like_ to only suppress the warnings for vendor code, since
        // we're not likely to fix those.  However, since:
        //
        //   * this seems to be hard
        //   * the consensus seems to be that these warnings are
        //     aren't terribly high value
        //   * training people to ignore warnings is pretty much always a bad
        //     plan
        //
        // we suppress them everywhere
        warnings: false
      }
    })
  ]
};
