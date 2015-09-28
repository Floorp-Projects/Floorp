/* eslint-env node */

// Webpack hurts to debug (lots of trial-and-error), so put non-trivial
// functionality in other script logic whenever practical.

var path = require("path");

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

  // We want the shared modules to be available without the requirer needing
  // to know the path to them, especially that path is different in
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
  }
};
