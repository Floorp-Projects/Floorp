/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const port = 8099;
const file = require("sdk/io/file");
const { pathFor } = require("sdk/system");
const { Loader } = require("sdk/test/loader");
const options = require("@test/options");

const loader = Loader(module);
const httpd = loader.require("sdk/test/httpd");
if (options.parseable || options.verbose)
  loader.sandbox("sdk/test/httpd").DEBUG = true;

exports.testBasicHTTPServer = function(test) {
  // Use the profile directory for the temporary file as that will be deleted
  // when tests are complete
  let basePath = pathFor("ProfD");
  let filePath = file.join(basePath, 'test-httpd.txt');
  let content = "This is the HTTPD test file.\n";
  let fileStream = file.open(filePath, 'w');
  fileStream.write(content);
  fileStream.close();

  let srv = httpd.startServerAsync(port, basePath);

  test.waitUntilDone();

  // Request this very file.
  let Request = require('sdk/request').Request;
  Request({
    url: "http://localhost:" + port + "/test-httpd.txt",
    onComplete: function (response) {
      test.assertEqual(response.text, content);
      done();
    }
  }).get();

  function done() {
    srv.stop(function() {
      test.done();
    });
  }
};

exports.testDynamicServer = function (test) {
  let content = "This is the HTTPD test file.\n";

  let srv = httpd.startServerAsync(port);

  // See documentation here:
  //http://doxygen.db48x.net/mozilla/html/interfacensIHttpServer.html#a81fc7e7e29d82aac5ce7d56d0bedfb3a
  //http://doxygen.db48x.net/mozilla/html/interfacensIHttpRequestHandler.html
  srv.registerPathHandler("/test-httpd.txt", function handle(request, response) {
    // Add text content type, only to avoid error in `Request` API
    response.setHeader("Content-Type", "text/plain", false);
    response.write(content);
  });

  test.waitUntilDone();

  // Request this very file.
  let Request = require('sdk/request').Request;
  Request({
    url: "http://localhost:" + port + "/test-httpd.txt",
    onComplete: function (response) {
      test.assertEqual(response.text, content);
      done();
    }
  }).get();

  function done() {
    srv.stop(function() {
      test.done();
    });
  }

}

exports.testAutomaticPortSelection = function (test) {
  const srv = httpd.startServerAsync(-1);

  test.waitUntilDone();

  const port = srv.identity.primaryPort;
  test.assert(0 <= port && port <= 65535);

  srv.stop(function() {
    test.done();
  });
}
