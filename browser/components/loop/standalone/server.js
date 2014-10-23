/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var express = require('express');
var app = express();

var port = process.env.PORT || 3000;
var loopServerPort = process.env.LOOP_SERVER_PORT || 5000;
var feedbackApiUrl = process.env.LOOP_FEEDBACK_API_URL ||
                     "https://input.allizom.org/api/v1/feedback";
var feedbackProductName = process.env.LOOP_FEEDBACK_PRODUCT_NAME || "Loop";

function getConfigFile(req, res) {
  "use strict";

  res.set('Content-Type', 'text/javascript');
  res.send([
    "var loop = loop || {};",
    "loop.config = loop.config || {};",
    "loop.config.serverUrl = 'http://localhost:" + loopServerPort + "';",
    "loop.config.feedbackApiUrl = '" + feedbackApiUrl + "';",
    "loop.config.feedbackProductName = '" + feedbackProductName + "';",
    // XXX Update with the real marketplace url once the FxOS Loop app is
    //     uploaded to the marketplace bug 1053424
    "loop.config.marketplaceUrl = 'http://fake-market.herokuapp.com/iframe-install.html'",
    "loop.config.brandWebsiteUrl = 'https://www.mozilla.org/firefox/';",
    "loop.config.privacyWebsiteUrl = 'https://www.mozilla.org/privacy';",
    "loop.config.legalWebsiteUrl = '/legal/terms';",
    "loop.config.fxosApp = loop.config.fxosApp || {};",
    "loop.config.fxosApp.name = 'Loop';",
    "loop.config.fxosApp.manifestUrl = 'http://fake-market.herokuapp.com/apps/packagedApp/manifest.webapp';"
  ].join("\n"));
}

app.get('/content/config.js', getConfigFile);
app.get('/content/c/config.js', getConfigFile);

// Various mappings to let us end up with:
// /test - for the test files
// /ui - for the ui showcase
// /content - for the standalone files.

app.use('/test', express.static(__dirname + '/../test'));
app.use('/ui', express.static(__dirname + '/../ui'));

// This exists exclusively for the unit tests. They are served the
// whole loop/ directory structure and expect some files in the standalone directory.
app.use('/standalone/content', express.static(__dirname + '/content'));

// We load /content this from  both /content *and* /../content. The first one
// does what we need for running in the github loop-client context, the second one
// handles running in the hg repo under mozilla-central and is used so that the shared
// files are in the right location.
app.use('/content', express.static(__dirname + '/content'));
app.use('/content', express.static(__dirname + '/../content'));
// These two are based on the above, but handle call urls, that have a /c/ in them.
app.use('/content/c', express.static(__dirname + '/content'));
app.use('/content/c', express.static(__dirname + '/../content'));

// As we don't have hashes on the urls, the best way to serve the index files
// appears to be to be to closely filter the url and match appropriately.
function serveIndex(req, res) {
  return res.sendfile(__dirname + '/content/index.html');
}

app.get(/^\/content\/[\w\-]+$/, serveIndex);
app.get(/^\/content\/c\/[\w\-]+$/, serveIndex);

var server = app.listen(port);

var baseUrl = "http://localhost:" + port + "/";

console.log("Static contents are available at " + baseUrl + "content/");
console.log("Tests are viewable at " + baseUrl + "test/");
console.log("Use this for development only.");

// Handle SIGTERM signal.
function shutdown(cb) {
  "use strict";

  try {
    server.close(function () {
      process.exit(0);
      if (cb !== undefined) {
        cb();
      }
    });

  } catch (ex) {
    console.log(ex + " while calling server.close)");
  }
}

process.on('SIGTERM', shutdown);
