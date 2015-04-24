/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Request } = require("sdk/request");
const { pathFor } = require("sdk/system");
const file = require("sdk/io/file");
const { URL } = require("sdk/url");
const { extend } = require("sdk/util/object");
const { Loader } = require("sdk/test/loader");
const options = require("sdk/test/options");

const loader = Loader(module);
const httpd = loader.require("./lib/httpd");
if (options.parseable || options.verbose)
  loader.sandbox("./lib/httpd").DEBUG = true;
const { startServerAsync } = httpd;

const { Cc, Ci, Cu } = require("chrome");
const { Services } = Cu.import("resource://gre/modules/Services.jsm");

// Use the profile directory for the temporary files as that will be deleted
// when tests are complete
const basePath = pathFor("ProfD");
const port = 8099;


exports.testOptionsValidator = function(assert) {
  // First, a simple test to make sure we didn't break normal functionality.
  assert.throws(function () {
    Request({
      url: null
    });
  }, /The option "url" is invalid./);

  // Next we'll have a Request that doesn't throw from c'tor, but from a setter.
  let req = Request({
    url: "http://playground.zpao.com/jetpack/request/text.php",
    onComplete: function () {}
  });
  assert.throws(function () {
    req.url = 'www.mozilla.org';
  }, /The option "url" is invalid/);
  // The url shouldn't have changed, so check that
  assert.equal(req.url, "http://playground.zpao.com/jetpack/request/text.php");

  // Test default anonymous parameter value
  assert.equal(req.anonymous, false);
  // Test set anonymous parameter value
  req = Request({
    url: "http://playground.zpao.com/jetpack/request/text.php",
    anonymous: true,
    onComplete: function () {}
  });
  assert.equal(req.anonymous, true);
  // Test wrong value as anonymous parameter value
  assert.throws(function() {
    Request({
      url: "http://playground.zpao.com/jetpack/request/text.php",
      anonymous: "invalidvalue"
    });
  }, /The option "anonymous" must be one of the following types/);
};

exports.testContentValidator = function(assert, done) {
  runMultipleURLs(null, assert, done, {
    url: "data:text/html;charset=utf-8,response",
    content: { 'key1' : null, 'key2' : 'some value' },
    onComplete: function(response) {
      assert.equal(response.text, "response?key1=null&key2=some+value");
    }
  });
};

// This is a request to a file that exists.
exports.testStatus200 = function (assert, done) {
  let srv = startServerAsync(port, basePath);
  let content = "Look ma, no hands!\n";
  let basename = "test-request.txt"
  let requestURL = "http://localhost:" + port + "/" + basename;

  prepareFile(basename, content);

  var req = Request({
    url: requestURL,
    onComplete: function (response) {
      assert.equal(this, req, "`this` should be request");
      assert.equal(response.status, 200);
      assert.equal(response.statusText, "OK");
      assert.equal(response.headers["Content-Type"], "text/plain");
      assert.equal(response.text, content);
      assert.strictEqual(response.url, requestURL);
      srv.stop(done);
    }
  }).get();
};

// Should return the location to which have been automatically redirected
exports.testRedirection = function (assert, done) {
  let srv = startServerAsync(port, basePath);
  
  let moveLocation = "test-request-302-located.txt";
  let movedContent = "here now\n";
  let movedUrl = "http://localhost:" + port + "/" + moveLocation;
  prepareFile(moveLocation, movedContent);

  let content = "The document has moved!\n";
  let contentHeaders = "HTTP 302 Found\nLocation: "+moveLocation+"\n";
  let basename = "test-request-302.txt"
  let requestURL = "http://localhost:" + port + "/" + basename;
  prepareFile(basename, content);
  prepareFile(basename+"^headers^", contentHeaders);

  var req = Request({
    url: requestURL,
    onComplete: function (response) {
      assert.equal(this, req, "`this` should be request");
      assert.equal(response.status, 200);
      assert.equal(response.statusText, "OK");
      assert.equal(response.headers["Content-Type"], "text/plain");
      assert.equal(response.text, movedContent);
      assert.strictEqual(response.url, movedUrl);
      srv.stop(done);
    }
  }).get();
};

// This tries to get a file that doesn't exist
exports.testStatus404 = function (assert, done) {
  var srv = startServerAsync(port, basePath);
  let requestURL = "http://localhost:" + port + "/test-request-404.txt";

  runMultipleURLs(srv, assert, done, {
    // the following URL doesn't exist
    url: requestURL,
    onComplete: function (response) {
      assert.equal(response.status, 404);
      assert.equal(response.statusText, "Not Found");
      assert.strictEqual(response.url, requestURL);
    }
  });
};

// a simple file with a known header
exports.testKnownHeader = function (assert, done) {
  var srv = startServerAsync(port, basePath);

 // Create the file that will be requested with the associated headers file
  let content = "This tests adding headers to the server's response.\n";
  let basename = "test-request-headers.txt";
  let headerContent = "x-jetpack-header: Jamba Juice\n";
  let headerBasename = "test-request-headers.txt^headers^";
  prepareFile(basename, content);
  prepareFile(headerBasename, headerContent);

  runMultipleURLs(srv, assert, done, {
    url: "http://localhost:" + port + "/test-request-headers.txt",
    onComplete: function (response) {
      assert.equal(response.headers["x-jetpack-header"], "Jamba Juice");
    }
  });
};

// complex headers
exports.testComplexHeader = function (assert, done) {
  let srv = startServerAsync(port, basePath);

  let basename = "test-request-complex-headers.sjs";
  let content = handleRequest.toString();
  prepareFile(basename, content);

  let headers = {
    "x-jetpack-header": "Jamba Juice is: delicious",
    "x-jetpack-header-2": "foo,bar",
    "x-jetpack-header-3": "sup dawg, i heard you like x, so we put a x in " +
      "yo x so you can y while you y",
    "Set-Cookie": "foo=bar\nbaz=foo"
  };

  runMultipleURLs(srv, assert, done, {
    url: "http://localhost:" + port + "/test-request-complex-headers.sjs",
    onComplete: function (response) {
      for (k in headers) {
        assert.equal(response.headers[k], headers[k]);
      }
    }
  });
};

// Force Allow Third Party cookies
exports.test3rdPartyCookies = function (assert, done) {
  let srv = startServerAsync(port, basePath);

  let basename = "test-request-3rd-party-cookies.sjs";

  // Function to handle the requests in the server
  let content = function handleRequest(request, response) {
    var cookiePresent = request.hasHeader("Cookie");
    // If no cookie, set it
    if(!cookiePresent) {
      response.setHeader("Set-Cookie", "cookie=monster;", "true");
      response.setHeader("x-jetpack-3rd-party", "false", "true");
    } else {
      // We got the cookie, say so
      response.setHeader("x-jetpack-3rd-party", "true", "true");
    }

    response.write("<html><body>This tests 3rd party cookies.</body></html>");
  }.toString();

  prepareFile(basename, content);

  // Disable the 3rd party cookies
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);

  Request({
    url: "http://localhost:" + port + "/test-request-3rd-party-cookies.sjs",
    onComplete: function (response) {
      // Check that the server created the cookie
      assert.equal(response.headers['Set-Cookie'], 'cookie=monster;');

      // Check it wasn't there before
      assert.equal(response.headers['x-jetpack-3rd-party'], 'false');

      // Make a second request, and check that the server this time
      // got the cookie
      Request({
        url: "http://localhost:" + port + "/test-request-3rd-party-cookies.sjs",
        onComplete: function (response) {
          assert.equal(response.headers['x-jetpack-3rd-party'], 'true');
          srv.stop(done);
        }
      }).get();
    }
  }).get();
};

// Test anonymous request behavior
exports.testAnonymousRequest = function(assert, done) {
  let srv = startServerAsync(port, basePath);
  let basename = "test-anonymous-request.sjs";
  let testUrl = "http://localhost:" + port + "/" + basename;
  // Function to handle the requests in the server
  let content = function handleRequest(request, response) {
    // Request to store cookie
    response.setHeader("Set-Cookie", "anonymousKey=anonymousValue;", "true");
    // Set response content type
    response.setHeader("Content-Type", "application/json");
    // Check if cookie was send during request
    var cookiePresent = request.hasHeader("Cookie");
    // Create server respone content
    response.write(JSON.stringify({ "hasCookie": cookiePresent }));
  }.toString();
  prepareFile(basename, content);
  // Create request callbacks
  var checkCookieCreated = function (response) {
    // Check that the server created the cookie
    assert.equal(response.headers['Set-Cookie'], 'anonymousKey=anonymousValue;');
    // Make an other request and check that the server this time got the cookie
    Request({
      url: testUrl,
      onComplete: checkCookieSend
    }).get();
  },
  checkCookieSend = function (response) {
    // Check the response sent headers and cookies
    assert.equal(response.anonymous, false);
    // Check the server got the created cookie
    assert.equal(response.json.hasCookie, true);
    // Make a anonymous request and check the server did not get the cookie
    Request({
      url: testUrl,
      anonymous: true,
      onComplete: checkCookieNotSend
    }).get();
  },
  checkCookieNotSend = function (response) {
    // Check the response is anonymous
    assert.equal(response.anonymous, true);
    // Check the server did not get the cookie
    assert.equal(response.json.hasCookie, false);
    // Stop the server
    srv.stop(done);
  };
  // Make the first request to create cookie
  Request({
    url: testUrl,
    onComplete: checkCookieCreated
  }).get();
};

exports.testSimpleJSON = function (assert, done) {
  let srv = startServerAsync(port, basePath);
  let json = { foo: "bar" };
  let basename = "test-request.json";
  prepareFile(basename, JSON.stringify(json));

  runMultipleURLs(srv, assert, done, {
    url: "http://localhost:" + port + "/" + basename,
    onComplete: function (response) {
      assert.deepEqual(response.json, json);
    }
  });
};

exports.testInvalidJSON = function (assert, done) {
  let srv = startServerAsync(port, basePath);
  let basename = "test-request-invalid.json";
  prepareFile(basename, '"this": "isn\'t JSON"');

  runMultipleURLs(srv, assert, done, {
    url: "http://localhost:" + port + "/" + basename,
    onComplete: function (response) {
      assert.equal(response.json, null);
    }
  });
};

exports.testDelete = function (assert, done) {
  let srv = startServerAsync(port, basePath);

  srv.registerPathHandler("/test-delete",
      function handle(request, response) {
    response.setHeader("Content-Type", "text/plain", false);
  });

  Request({
    url: "http://localhost:" + port + "/test-delete",
    onComplete: function (response) {
      // We cannot access the METHOD of the request to verify it's set
      // correctly.
      assert.equal(response.text, "");
      assert.equal(response.statusText, "OK");
      assert.equal(response.headers["Content-Type"], "text/plain");
      srv.stop(done);
    }
  }).delete();
};

exports.testHead = function (assert, done) {
  let srv = startServerAsync(port, basePath);

  srv.registerPathHandler("/test-head",
      function handle(request, response) {
    response.setHeader("Content-Type", "text/plain", false);
  });

  Request({
    url: "http://localhost:" + port + "/test-head",
    onComplete: function (response) {
      assert.equal(response.text, "");
      assert.equal(response.statusText, "OK");
      assert.equal(response.headers["Content-Type"], "text/plain");
      srv.stop(done);
    }
  }).head();
};

function runMultipleURLs (srv, assert, done, options) {
  let urls = [options.url, URL(options.url)];
  let cb = options.onComplete;
  let ran = 0;
  let onComplete = function (res) {
    cb(res);
    if (++ran === urls.length)
      srv ? srv.stop(done) : done();
  };
  urls.forEach(function (url) {
    Request(extend(options, { url: url, onComplete: onComplete })).get();
  });
}

// All tests below here require a network connection. They will be commented out
// when checked in. If you'd like to run them, simply uncomment them.
//
// When we have the means, these tests will be converted so that they don't
// require an external server nor a network connection.

/*
exports.testGetWithParamsNotContent = function (assert, done) {
  Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php?foo=bar",
    onComplete: function (response) {
      let expected = {
        "POST": [],
        "GET" : { foo: "bar" }
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).get();
}

exports.testGetWithContent = function (assert, done) {
  Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php",
    content: { foo: "bar" },
    onComplete: function (response) {
      let expected = {
        "POST": [],
        "GET" : { foo: "bar" }
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).get();
}

exports.testGetWithParamsAndContent = function (assert, done) {
  Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php?foo=bar",
    content: { baz: "foo" },
    onComplete: function (response) {
      let expected = {
        "POST": [],
        "GET" : { foo: "bar", baz: "foo" }
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).get();
}

exports.testSimplePost = function (assert, done) {
  Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php",
    content: { foo: "bar" },
    onComplete: function (response) {
      let expected = {
        "POST": { foo: "bar" },
        "GET" : []
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).post();
}

exports.testEncodedContent = function (assert, done) {
  Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php",
    content: "foo=bar&baz=foo",
    onComplete: function (response) {
      let expected = {
        "POST": [],
        "GET" : { foo: "bar", baz: "foo" }
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).get();
}

exports.testEncodedContentWithSpaces = function (assert, done) {
  Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php",
    content: "foo=bar+hop!&baz=foo",
    onComplete: function (response) {
      let expected = {
        "POST": [],
        "GET" : { foo: "bar hop!", baz: "foo" }
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).get();
}

exports.testGetWithArray = function (assert, done) {
  Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php",
    content: { foo: [1, 2], baz: "foo" },
    onComplete: function (response) {
      let expected = {
        "POST": [],
        "GET" : { foo: [1, 2], baz: "foo" }
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).get();
}

exports.testGetWithNestedArray = function (assert, done) {
  Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php",
    content: { foo: [1, 2, [3, 4]], bar: "baz" },
    onComplete: function (response) {
      let expected = {
        "POST": [],
        "GET" : this.content
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).get();
}

exports.testGetWithNestedArray = function (assert, done) {
  let request = Request({
    url: "http://playground.zpao.com/jetpack/request/getpost.php",
    content: {
      foo: [1, 2, {
        omg: "bbq",
        "all your base!": "are belong to us"
      }],
      bar: "baz"
    },
    onComplete: function (response) {
      let expected = {
        "POST": [],
        "GET" : request.content
      };
      assert.deepEqual(response.json, expected);
      done();
    }
  }).get();
}
*/

function prepareFile(basename, content) {
  let filePath = file.join(basePath, basename);
  let fileStream = file.open(filePath, 'w');
  fileStream.write(content);
  fileStream.close();
}

// Helper function for testComplexHeaders
function handleRequest(request, response) {
  // Test header with an extra colon
  response.setHeader("x-jetpack-header", "Jamba Juice is: delicious", "true");

  // Test that multiple headers with the same name coalesce
  response.setHeader("x-jetpack-header-2", "foo", "true");
  response.setHeader("x-jetpack-header-2", "bar", "true");

  // Test that headers with commas work
  response.setHeader("x-jetpack-header-3", "sup dawg, i heard you like x, " +
    "so we put a x in yo x so you can y while you y", "true");

  // Test that multiple cookies work
  response.setHeader("Set-Cookie", "foo=bar", "true");
  response.setHeader("Set-Cookie", "baz=foo", "true");

  response.write("<html><body>This file tests more complex headers.</body></html>");
}

require('sdk/test').run(exports);
