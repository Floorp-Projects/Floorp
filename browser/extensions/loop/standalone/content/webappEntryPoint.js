// When processed by webpack, this file bundles up all the JS for the
// standalone client.
//
// When loaded by the development server, index.html supplies a require
// function to load these.

// Right now, these are manually ordered so that all dependencies are
// satisfied.  Before long, we'd like to convert these into real modules
// and/or better shims, and push the dependencies down into the modules
// themselves so that this manual management step goes away.

// To get started, we're using webpack's script loader to load these things in
// as-is.

/* global require */

// The OpenTok SDK tries to do some heuristic detection of require and
// assumes a node environment if it's present, which confuses webpack, so
// we turn that off by forcing require to false in that context.
require("imports?require=>false!shared/libs/sdk.js");

// Ultimately, we'll likely want to pull the vendor libraries from npm, as that
// makes upgrading easier, and it's generally better practice to minify the
// "source" versions of libraries rather than built artifacts.  We probably do
// want to minify them ourselves since this allows for better dead-code
// elimination, but that can be a bit of judgement call.
require("exports?_!shared/libs/lodash-3.9.3.js");

// Disable Backbone's AMD auto-detection, as described at:
//
// https://github.com/jashkenas/backbone/wiki/Using-Backbone-without-jQuery
//
require("expose?Backbone!imports?define=>false!shared/libs/backbone-1.2.1.js");

/* global: __PROD__ */
if (typeof __PROD__ !== "undefined") {
  // webpack warns if we try to minify some prebuilt libraries, so we
  // pull in the unbuilt version from node_modules
  require("expose?React!react");
  require("expose?React!react/addons");
  require("expose?classNames!classnames");
} else {
  // our development server setup doesn't yet handle real modules, so for now...
  require("shared/libs/react-0.13.3.js");
  require("shared/libs/classnames-2.2.0.js");
}


// Someday, these will be real modules.  For now, we're chaining loaders
// to teach webpack how to treat them like modules anyway.
//
// We do it in this file rather than globally in webpack.config.js
// because:
//
// * it's easiest to understand magic (like loader chaining
//   interactions) when it's explicit and in one place
// * migrating stuff over to real modules is easier to do gradually
//
// The strategy works like this (webpack loaders chain from right to left):
//
// In standalone, loop is defined for the first time in index.html.
//
// The exports?loop loader sets up webpack to ensure that exports is
// actually exposed to outside world, rather than held privately.
//
// The imports=?loop=>window.loop loader puts the existing window.loop
// reference into that exported container for further modification.
//
// See https://webpack.github.io/docs/shimming-modules.html for more
// context.
//
require("imports?loop=>window.loop!exports?loop!shared/js/loopapi-client.js");
require("imports?loop=>window.loop!exports?loop!shared/js/utils.js");
require("imports?this=>window,loop=>window.loop!exports?loop!shared/js/crypto.js");
require("imports?loop=>window.loop!exports?loop!shared/js/mixins.js");
require("imports?loop=>window.loop!exports?loop!shared/js/actions.js");
require("imports?loop=>window.loop!exports?loop!shared/js/validate.js");
require("imports?loop=>window.loop!exports?loop!shared/js/dispatcher.js");
require("imports?loop=>window.loop!exports?loop!shared/js/otSdkDriver.js");
require("imports?loop=>window.loop!exports?loop!shared/js/store.js");
require("imports?loop=>window.loop!exports?loop!shared/js/activeRoomStore.js");
require("imports?loop=>window.loop!exports?loop!shared/js/views.js");
require("imports?loop=>window.loop!exports?loop!shared/js/urlRegExps.js");
require("imports?loop=>window.loop!exports?loop!shared/js/textChatStore.js");
require("imports?loop=>window.loop!exports?loop!shared/js/textChatView.js");
require("imports?loop=>window.loop!exports?loop!shared/js/linkifiedTextView.js");

require("imports?loop=>window.loop!exports?loop!./js/standaloneAppStore.js");
require("imports?loop=>window.loop!exports?loop!./js/standaloneMozLoop.js");
require("imports?loop=>window.loop!exports?loop!./js/standaloneRoomViews.js");
require("imports?loop=>window.loop!exports?loop!./js/standaloneMetricsStore.js");
require("imports?loop=>window.loop!exports?loop!./js/webapp.js");
