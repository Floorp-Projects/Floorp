var path = "/tests/dom/base/test/";

function isOpaqueResponse(response) {
  return (
    response.type == "opaque" &&
    response.status === 0 &&
    response.statusText === ""
  );
}

function testModeSameOrigin() {
  // Fetch spec Section 4, step 4, "request's mode is same-origin".
  var req = new Request("http://example.com", { mode: "same-origin" });
  return fetch(req).then(
    function(res) {
      ok(
        false,
        "Attempting to fetch a resource from a different origin with mode same-origin should fail."
      );
    },
    function(e) {
      ok(
        e instanceof TypeError,
        "Attempting to fetch a resource from a different origin with mode same-origin should fail."
      );
    }
  );
}

function testNoCorsCtor() {
  // Request constructor Step 19.1
  var simpleMethods = ["GET", "HEAD", "POST"];
  for (var i = 0; i < simpleMethods.length; ++i) {
    var r = new Request("http://example.com", {
      method: simpleMethods[i],
      mode: "no-cors",
    });
    ok(
      true,
      "no-cors Request with simple method " + simpleMethods[i] + " is allowed."
    );
  }

  var otherMethods = ["DELETE", "OPTIONS", "PUT"];
  for (var i = 0; i < otherMethods.length; ++i) {
    try {
      var r = new Request("http://example.com", {
        method: otherMethods[i],
        mode: "no-cors",
      });
      ok(
        false,
        "no-cors Request with non-simple method " +
          otherMethods[i] +
          " is not allowed."
      );
    } catch (e) {
      ok(
        true,
        "no-cors Request with non-simple method " +
          otherMethods[i] +
          " is not allowed."
      );
    }
  }

  // Request constructor Step 19.2, check guarded headers.
  var r = new Request(".", { mode: "no-cors" });
  r.headers.append("Content-Type", "multipart/form-data");
  is(
    r.headers.get("content-type"),
    "multipart/form-data",
    "Appending simple header should succeed"
  );
  r.headers.append("custom", "value");
  ok(!r.headers.has("custom"), "Appending custom header should fail");
  r.headers.append("DNT", "value");
  ok(!r.headers.has("DNT"), "Appending forbidden header should fail");
}

var corsServerPath =
  "/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?";
function testModeNoCors() {
  // Fetch spec, section 4, step 4, response tainting should be set opaque, so
  // that fetching leads to an opaque filtered response in step 8.
  var r = new Request("http://example.com" + corsServerPath + "status=200", {
    mode: "no-cors",
  });
  return fetch(r).then(
    function(res) {
      ok(
        isOpaqueResponse(res),
        "no-cors Request fetch should result in opaque response"
      );
    },
    function(e) {
      ok(false, "no-cors Request fetch should not error");
    }
  );
}

function testSameOriginCredentials() {
  var cookieStr = "type=chocolatechip";
  var tests = [
    {
      // Initialize by setting a cookie.
      pass: 1,
      setCookie: cookieStr,
      withCred: "same-origin",
    },
    {
      // Default mode is "same-origin".
      pass: 1,
      cookie: cookieStr,
    },
    {
      pass: 1,
      noCookie: 1,
      withCred: "omit",
    },
    {
      pass: 1,
      cookie: cookieStr,
      withCred: "same-origin",
    },
    {
      pass: 1,
      cookie: cookieStr,
      withCred: "include",
    },
  ];

  var finalPromiseResolve, finalPromiseReject;
  var finalPromise = new Promise(function(res, rej) {
    finalPromiseResolve = res;
    finalPromiseReject = rej;
  });

  function makeRequest(test) {
    req = {
      // Add a default query param just to make formatting the actual params
      // easier.
      url: corsServerPath + "a=b",
      method: test.method,
      headers: test.headers,
      withCred: test.withCred,
    };

    if (test.setCookie) {
      req.url += "&setCookie=" + escape(test.setCookie);
    }
    if (test.cookie) {
      req.url += "&cookie=" + escape(test.cookie);
    }
    if (test.noCookie) {
      req.url += "&noCookie";
    }

    return new Request(req.url, {
      method: req.method,
      headers: req.headers,
      credentials: req.withCred,
    });
  }

  function testResponse(res, test) {
    ok(test.pass, "Expected test to pass " + JSON.stringify(test));
    is(res.status, 200, "wrong status in test for " + JSON.stringify(test));
    is(res.statusText, "OK", "wrong status text for " + JSON.stringify(test));
    return res.text().then(function(v) {
      is(
        v,
        "<res>hello pass</res>\n",
        "wrong text in test for " + JSON.stringify(test)
      );
    });
  }

  function runATest(tests, i) {
    var test = tests[i];
    var request = makeRequest(test);
    console.log(request.url);
    fetch(request).then(
      function(res) {
        testResponse(res, test).then(function() {
          if (i < tests.length - 1) {
            runATest(tests, i + 1);
          } else {
            finalPromiseResolve();
          }
        });
      },
      function(e) {
        ok(!test.pass, "Expected test to fail " + JSON.stringify(test));
        ok(e instanceof TypeError, "Test should fail " + JSON.stringify(test));
        if (i < tests.length - 1) {
          runATest(tests, i + 1);
        } else {
          finalPromiseResolve();
        }
      }
    );
  }

  runATest(tests, 0);
  return finalPromise;
}

function testModeCors() {
  var tests = [
    // Plain request
    { pass: 1, method: "GET", noAllowPreflight: 1 },

    // undefined username
    { pass: 1, method: "GET", noAllowPreflight: 1, username: undefined },

    // undefined username and password
    {
      pass: 1,
      method: "GET",
      noAllowPreflight: 1,
      username: undefined,
      password: undefined,
    },

    // nonempty username
    { pass: 0, method: "GET", noAllowPreflight: 1, username: "user" },

    // nonempty password
    { pass: 0, method: "GET", noAllowPreflight: 1, password: "password" },

    // Default allowed headers
    {
      pass: 1,
      method: "GET",
      headers: {
        "Content-Type": "text/plain",
        Accept: "foo/bar",
        "Accept-Language": "sv-SE",
      },
      noAllowPreflight: 1,
    },

    {
      pass: 0,
      method: "GET",
      headers: {
        "Content-Type": "foo/bar",
        Accept: "foo/bar",
        "Accept-Language": "sv-SE",
      },
      noAllowPreflight: 1,
    },

    {
      pass: 0,
      method: "GET",
      headers: { "Content-Type": "foo/bar, text/plain" },
      noAllowPreflight: 1,
    },

    {
      pass: 0,
      method: "GET",
      headers: { "Content-Type": "foo/bar, text/plain, garbage" },
      noAllowPreflight: 1,
    },

    // Custom headers
    {
      pass: 1,
      method: "GET",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "x-my-header",
    },
    {
      pass: 1,
      method: "GET",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "X-My-Header",
    },
    {
      pass: 1,
      method: "GET",
      headers: {
        "x-my-header": "myValue",
        "long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header":
          "secondValue",
      },
      allowHeaders:
        "x-my-header, long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header-long-header",
    },
    {
      pass: 1,
      method: "GET",
      headers: { "x-my%-header": "myValue" },
      allowHeaders: "x-my%-header",
    },
    { pass: 0, method: "GET", headers: { "x-my-header": "myValue" } },
    { pass: 0, method: "GET", headers: { "x-my-header": "" } },
    {
      pass: 0,
      method: "GET",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "",
    },
    {
      pass: 0,
      method: "GET",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "y-my-header",
    },
    {
      pass: 0,
      method: "GET",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "x-my-header y-my-header",
    },
    {
      pass: 0,
      method: "GET",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "x-my-header, y-my-header z",
    },
    {
      pass: 0,
      method: "GET",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "x-my-header, y-my-he(ader",
    },
    {
      pass: 0,
      method: "GET",
      headers: { myheader: "" },
      allowMethods: "myheader",
    },
    {
      pass: 1,
      method: "GET",
      headers: { "User-Agent": "myValue" },
      allowHeaders: "User-Agent",
    },
    { pass: 0, method: "GET", headers: { "User-Agent": "myValue" } },

    // Multiple custom headers
    {
      pass: 1,
      method: "GET",
      headers: {
        "x-my-header": "myValue",
        "second-header": "secondValue",
        "third-header": "thirdValue",
      },
      allowHeaders: "x-my-header, second-header, third-header",
    },
    {
      pass: 1,
      method: "GET",
      headers: {
        "x-my-header": "myValue",
        "second-header": "secondValue",
        "third-header": "thirdValue",
      },
      allowHeaders: "x-my-header,second-header,third-header",
    },
    {
      pass: 1,
      method: "GET",
      headers: {
        "x-my-header": "myValue",
        "second-header": "secondValue",
        "third-header": "thirdValue",
      },
      allowHeaders: "x-my-header ,second-header ,third-header",
    },
    {
      pass: 1,
      method: "GET",
      headers: {
        "x-my-header": "myValue",
        "second-header": "secondValue",
        "third-header": "thirdValue",
      },
      allowHeaders: "x-my-header , second-header , third-header",
    },
    {
      pass: 1,
      method: "GET",
      headers: { "x-my-header": "myValue", "second-header": "secondValue" },
      allowHeaders: ",  x-my-header, , ,, second-header, ,   ",
    },
    {
      pass: 1,
      method: "GET",
      headers: { "x-my-header": "myValue", "second-header": "secondValue" },
      allowHeaders: "x-my-header, second-header, unused-header",
    },
    {
      pass: 0,
      method: "GET",
      headers: { "x-my-header": "myValue", "y-my-header": "secondValue" },
      allowHeaders: "x-my-header",
    },
    {
      pass: 0,
      method: "GET",
      headers: { "x-my-header": "", "y-my-header": "" },
      allowHeaders: "x-my-header",
    },

    // HEAD requests
    { pass: 1, method: "HEAD", noAllowPreflight: 1 },

    // HEAD with safe headers
    {
      pass: 1,
      method: "HEAD",
      headers: {
        "Content-Type": "text/plain",
        Accept: "foo/bar",
        "Accept-Language": "sv-SE",
      },
      noAllowPreflight: 1,
    },
    {
      pass: 0,
      method: "HEAD",
      headers: {
        "Content-Type": "foo/bar",
        Accept: "foo/bar",
        "Accept-Language": "sv-SE",
      },
      noAllowPreflight: 1,
    },
    {
      pass: 0,
      method: "HEAD",
      headers: { "Content-Type": "foo/bar, text/plain" },
      noAllowPreflight: 1,
    },
    {
      pass: 0,
      method: "HEAD",
      headers: { "Content-Type": "foo/bar, text/plain, garbage" },
      noAllowPreflight: 1,
    },

    // HEAD with custom headers
    {
      pass: 1,
      method: "HEAD",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "x-my-header",
    },
    { pass: 0, method: "HEAD", headers: { "x-my-header": "myValue" } },
    {
      pass: 0,
      method: "HEAD",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "",
    },
    {
      pass: 0,
      method: "HEAD",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "y-my-header",
    },
    {
      pass: 0,
      method: "HEAD",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "x-my-header y-my-header",
    },

    // POST tests
    { pass: 1, method: "POST", body: "hi there", noAllowPreflight: 1 },
    { pass: 1, method: "POST" },
    { pass: 1, method: "POST", noAllowPreflight: 1 },

    // POST with standard headers
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain" },
      noAllowPreflight: 1,
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "multipart/form-data" },
      noAllowPreflight: 1,
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      noAllowPreflight: 1,
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "foo/bar" },
    },
    { pass: 0, method: "POST", headers: { "Content-Type": "foo/bar" } },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: {
        "Content-Type": "text/plain",
        Accept: "foo/bar",
        "Accept-Language": "sv-SE",
      },
      noAllowPreflight: 1,
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "foo/bar, text/plain" },
      noAllowPreflight: 1,
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "foo/bar, text/plain, garbage" },
      noAllowPreflight: 1,
    },

    // POST with custom headers
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: {
        Accept: "foo/bar",
        "Accept-Language": "sv-SE",
        "x-my-header": "myValue",
      },
      allowHeaders: "x-my-header",
    },
    {
      pass: 1,
      method: "POST",
      headers: { "Content-Type": "text/plain", "x-my-header": "myValue" },
      allowHeaders: "x-my-header",
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain", "x-my-header": "myValue" },
      allowHeaders: "x-my-header",
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "foo/bar", "x-my-header": "myValue" },
      allowHeaders: "x-my-header, content-type",
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "foo/bar" },
      noAllowPreflight: 1,
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "foo/bar", "x-my-header": "myValue" },
      allowHeaders: "x-my-header",
    },
    {
      pass: 1,
      method: "POST",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "x-my-header",
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "x-my-header": "myValue" },
      allowHeaders: "x-my-header, $_%",
    },

    // Other methods
    { pass: 1, method: "DELETE", allowMethods: "DELETE" },
    { pass: 0, method: "DELETE", allowHeaders: "DELETE" },
    { pass: 0, method: "DELETE" },
    { pass: 0, method: "DELETE", allowMethods: "" },
    { pass: 1, method: "DELETE", allowMethods: "POST, PUT, DELETE" },
    { pass: 1, method: "DELETE", allowMethods: "POST, DELETE, PUT" },
    { pass: 1, method: "DELETE", allowMethods: "DELETE, POST, PUT" },
    { pass: 1, method: "DELETE", allowMethods: "POST ,PUT ,DELETE" },
    { pass: 1, method: "DELETE", allowMethods: "POST,PUT,DELETE" },
    { pass: 1, method: "DELETE", allowMethods: "POST , PUT , DELETE" },
    {
      pass: 1,
      method: "DELETE",
      allowMethods: "  ,,  PUT ,,  ,    , DELETE  ,  ,",
    },
    { pass: 0, method: "DELETE", allowMethods: "PUT" },
    { pass: 0, method: "DELETE", allowMethods: "DELETEZ" },
    { pass: 0, method: "DELETE", allowMethods: "DELETE PUT" },
    { pass: 0, method: "DELETE", allowMethods: "DELETE, PUT Z" },
    { pass: 0, method: "DELETE", allowMethods: "DELETE, PU(T" },
    { pass: 0, method: "DELETE", allowMethods: "PUT DELETE" },
    { pass: 0, method: "DELETE", allowMethods: "PUT Z, DELETE" },
    { pass: 0, method: "DELETE", allowMethods: "PU(T, DELETE" },
    { pass: 0, method: "PUT", allowMethods: "put" },

    // Status messages
    {
      pass: 1,
      method: "GET",
      noAllowPreflight: 1,
      status: 404,
      statusMessage: "nothin' here",
    },
    {
      pass: 1,
      method: "GET",
      noAllowPreflight: 1,
      status: 401,
      statusMessage: "no can do",
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "foo/bar" },
      allowHeaders: "content-type",
      status: 500,
      statusMessage: "server boo",
    },
    {
      pass: 1,
      method: "GET",
      noAllowPreflight: 1,
      status: 200,
      statusMessage: "Yes!!",
    },
    {
      pass: 0,
      method: "GET",
      headers: { "x-my-header": "header value" },
      allowHeaders: "x-my-header",
      preflightStatus: 400,
    },
    {
      pass: 1,
      method: "GET",
      headers: { "x-my-header": "header value" },
      allowHeaders: "x-my-header",
      preflightStatus: 200,
    },
    {
      pass: 1,
      method: "GET",
      headers: { "x-my-header": "header value" },
      allowHeaders: "x-my-header",
      preflightStatus: 204,
    },

    // exposed headers
    {
      pass: 1,
      method: "GET",
      responseHeaders: { "x-my-header": "x header" },
      exposeHeaders: "x-my-header",
      expectedResponseHeaders: ["x-my-header"],
    },
    {
      pass: 0,
      method: "GET",
      origin: "http://invalid",
      responseHeaders: { "x-my-header": "x header" },
      exposeHeaders: "x-my-header",
      expectedResponseHeaders: [],
    },
    {
      pass: 1,
      method: "GET",
      responseHeaders: { "x-my-header": "x header" },
      expectedResponseHeaders: [],
    },
    {
      pass: 1,
      method: "GET",
      responseHeaders: { "x-my-header": "x header" },
      exposeHeaders: "x-my-header y",
      expectedResponseHeaders: [],
    },
    {
      pass: 1,
      method: "GET",
      responseHeaders: { "x-my-header": "x header" },
      exposeHeaders: "y x-my-header",
      expectedResponseHeaders: [],
    },
    {
      pass: 1,
      method: "GET",
      responseHeaders: { "x-my-header": "x header" },
      exposeHeaders: "x-my-header, y-my-header z",
      expectedResponseHeaders: [],
    },
    {
      pass: 1,
      method: "GET",
      responseHeaders: { "x-my-header": "x header" },
      exposeHeaders: "x-my-header, y-my-hea(er",
      expectedResponseHeaders: [],
    },
    {
      pass: 1,
      method: "GET",
      responseHeaders: { "x-my-header": "x header", "y-my-header": "y header" },
      exposeHeaders: "  ,  ,,y-my-header,z-my-header,  ",
      expectedResponseHeaders: ["y-my-header"],
    },
    {
      pass: 1,
      method: "GET",
      responseHeaders: {
        "Cache-Control": "cacheControl header",
        "Content-Language": "contentLanguage header",
        Expires: "expires header",
        "Last-Modified": "lastModified header",
        Pragma: "pragma header",
        Unexpected: "unexpected header",
      },
      expectedResponseHeaders: [
        "Cache-Control",
        "Content-Language",
        "Content-Type",
        "Expires",
        "Last-Modified",
        "Pragma",
      ],
    },
    // Check that sending a body in the OPTIONS response works
    {
      pass: 1,
      method: "DELETE",
      allowMethods: "DELETE",
      preflightBody: "I'm a preflight response body",
    },
  ];

  var baseURL = "http://example.org" + corsServerPath;
  var origin = "http://mochi.test:8888";
  var fetches = [];
  for (test of tests) {
    var req = {
      url: baseURL + "allowOrigin=" + escape(test.origin || origin),
      method: test.method,
      headers: test.headers,
      uploadProgress: test.uploadProgress,
      body: test.body,
      responseHeaders: test.responseHeaders,
    };

    if (test.pass) {
      req.url += "&origin=" + escape(origin) + "&requestMethod=" + test.method;
    }

    if ("username" in test) {
      var u = new URL(req.url);
      u.username = test.username || "";
      req.url = u.href;
    }

    if ("password" in test) {
      var u = new URL(req.url);
      u.password = test.password || "";
      req.url = u.href;
    }

    if (test.noAllowPreflight) {
      req.url += "&noAllowPreflight";
    }

    if (test.pass && "headers" in test) {
      function isUnsafeHeader(name) {
        lName = name.toLowerCase();
        return (
          lName != "accept" &&
          lName != "accept-language" &&
          (lName != "content-type" ||
            ![
              "text/plain",
              "multipart/form-data",
              "application/x-www-form-urlencoded",
            ].includes(test.headers[name].toLowerCase()))
        );
      }
      req.url += "&headers=" + escape(JSON.stringify(test.headers));
      reqHeaders = escape(
        Object.keys(test.headers)
          .filter(isUnsafeHeader)
          .map(s => s.toLowerCase())
          .sort()
          .join(",")
      );
      req.url += reqHeaders ? "&requestHeaders=" + reqHeaders : "";
    }
    if ("allowHeaders" in test) {
      req.url += "&allowHeaders=" + escape(test.allowHeaders);
    }
    if ("allowMethods" in test) {
      req.url += "&allowMethods=" + escape(test.allowMethods);
    }
    if (test.body) {
      req.url += "&body=" + escape(test.body);
    }
    if (test.status) {
      req.url += "&status=" + test.status;
      req.url += "&statusMessage=" + escape(test.statusMessage);
    }
    if (test.preflightStatus) {
      req.url += "&preflightStatus=" + test.preflightStatus;
    }
    if (test.responseHeaders) {
      req.url +=
        "&responseHeaders=" + escape(JSON.stringify(test.responseHeaders));
    }
    if (test.exposeHeaders) {
      req.url += "&exposeHeaders=" + escape(test.exposeHeaders);
    }
    if (test.preflightBody) {
      req.url += "&preflightBody=" + escape(test.preflightBody);
    }

    fetches.push(
      (function(test) {
        return new Promise(function(resolve) {
          resolve(
            new Request(req.url, {
              method: req.method,
              mode: "cors",
              headers: req.headers,
              body: req.body,
            })
          );
        })
          .then(function(request) {
            return fetch(request);
          })
          .then(function(res) {
            ok(test.pass, "Expected test to pass for " + JSON.stringify(test));
            if (test.status) {
              is(
                res.status,
                test.status,
                "wrong status in test for " + JSON.stringify(test)
              );
              is(
                res.statusText,
                test.statusMessage,
                "wrong status text for " + JSON.stringify(test)
              );
            } else {
              is(
                res.status,
                200,
                "wrong status in test for " + JSON.stringify(test)
              );
              is(
                res.statusText,
                "OK",
                "wrong status text for " + JSON.stringify(test)
              );
            }
            if (test.responseHeaders) {
              for (header in test.responseHeaders) {
                if (!test.expectedResponseHeaders.includes(header)) {
                  is(
                    res.headers.has(header),
                    false,
                    "|Headers.has()|wrong response header (" +
                      header +
                      ") in test for " +
                      JSON.stringify(test)
                  );
                } else {
                  is(
                    res.headers.get(header),
                    test.responseHeaders[header],
                    "|Headers.get()|wrong response header (" +
                      header +
                      ") in test for " +
                      JSON.stringify(test)
                  );
                }
              }
            }

            return res.text();
          })
          .then(function(v) {
            if (test.method !== "HEAD") {
              is(
                v,
                "<res>hello pass</res>\n",
                "wrong responseText in test for " + JSON.stringify(test)
              );
            } else {
              is(
                v,
                "",
                "wrong responseText in HEAD test for " + JSON.stringify(test)
              );
            }
          })
          .catch(function(e) {
            ok(!test.pass, "Expected test failure for " + JSON.stringify(test));
            ok(
              e instanceof TypeError,
              "Exception should be TypeError for " + JSON.stringify(test)
            );
          });
      })(test)
    );
  }

  return Promise.all(fetches);
}

function testCrossOriginCredentials() {
  var origin = "http://mochi.test:8888";
  var tests = [
    { pass: 1, method: "GET", withCred: "include", allowCred: 1 },
    { pass: 0, method: "GET", withCred: "include", allowCred: 0 },
    { pass: 0, method: "GET", withCred: "include", allowCred: 1, origin: "*" },
    { pass: 1, method: "GET", withCred: "omit", allowCred: 1, origin: "*" },
    {
      pass: 1,
      method: "GET",
      setCookie: "a=1",
      withCred: "include",
      allowCred: 1,
    },
    {
      pass: 1,
      method: "GET",
      cookie: "a=1",
      withCred: "include",
      allowCred: 1,
    },
    { pass: 1, method: "GET", noCookie: 1, withCred: "omit", allowCred: 1 },
    { pass: 0, method: "GET", noCookie: 1, withCred: "include", allowCred: 1 },
    {
      pass: 1,
      method: "GET",
      setCookie: "a=2",
      withCred: "omit",
      allowCred: 1,
    },
    {
      pass: 1,
      method: "GET",
      cookie: "a=1",
      withCred: "include",
      allowCred: 1,
    },
    {
      pass: 1,
      method: "GET",
      setCookie: "a=2",
      withCred: "include",
      allowCred: 1,
    },
    {
      pass: 1,
      method: "GET",
      cookie: "a=2",
      withCred: "include",
      allowCred: 1,
    },
    {
      // When credentials mode is same-origin, but mode is cors, no
      // cookie should be sent cross origin.
      pass: 0,
      method: "GET",
      cookie: "a=2",
      withCred: "same-origin",
      allowCred: 1,
    },
    {
      // When credentials mode is same-origin, but mode is cors, no
      // cookie should be sent cross origin. This test checks the same
      // thing as above, but uses the noCookie check on the server
      // instead, and expects a valid response.
      pass: 1,
      method: "GET",
      noCookie: 1,
      withCred: "same-origin",
    },
    {
      // Initialize by setting a cookies for same- and cross- origins.
      pass: 1,
      hops: [
        { server: origin, setCookie: escape("a=1") },
        {
          server: "http://example.com",
          allowOrigin: origin,
          allowCred: 1,
          setCookie: escape("a=2"),
        },
      ],
      withCred: "include",
    },
    {
      pass: 1,
      method: "GET",
      hops: [
        { server: origin, cookie: escape("a=1") },
        { server: origin, cookie: escape("a=1") },
        { server: "http://example.com", allowOrigin: origin, noCookie: 1 },
      ],
      withCred: "same-origin",
    },
    {
      pass: 1,
      method: "GET",
      hops: [
        { server: origin, cookie: escape("a=1") },
        { server: origin, cookie: escape("a=1") },
        {
          server: "http://example.com",
          allowOrigin: origin,
          allowCred: 1,
          cookie: escape("a=2"),
        },
      ],
      withCred: "include",
    },
    {
      pass: 1,
      method: "GET",
      hops: [
        { server: origin, cookie: escape("a=1") },
        { server: origin, cookie: escape("a=1") },
        { server: "http://example.com", allowOrigin: "*", noCookie: 1 },
      ],
      withCred: "same-origin",
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: origin, cookie: escape("a=1") },
        { server: origin, cookie: escape("a=1") },
        {
          server: "http://example.com",
          allowOrigin: "*",
          allowCred: 1,
          cookie: escape("a=2"),
        },
      ],
      withCred: "include",
    },
    // fails because allow-credentials CORS header is not set by server
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: origin, cookie: escape("a=1") },
        { server: origin, cookie: escape("a=1") },
        {
          server: "http://example.com",
          allowOrigin: origin,
          cookie: escape("a=2"),
        },
      ],
      withCred: "include",
    },
    {
      pass: 1,
      method: "GET",
      hops: [
        { server: origin, noCookie: 1 },
        { server: origin, noCookie: 1 },
        { server: "http://example.com", allowOrigin: origin, noCookie: 1 },
      ],
      withCred: "omit",
    },
  ];

  var baseURL = "http://example.org" + corsServerPath;
  var origin = "http://mochi.test:8888";

  var finalPromiseResolve, finalPromiseReject;
  var finalPromise = new Promise(function(res, rej) {
    finalPromiseResolve = res;
    finalPromiseReject = rej;
  });

  function makeRequest(test) {
    var url;
    if (test.hops) {
      url =
        test.hops[0].server +
        corsServerPath +
        "hop=1&hops=" +
        escape(JSON.stringify(test.hops));
    } else {
      url = baseURL + "allowOrigin=" + escape(test.origin || origin);
    }
    req = {
      url,
      method: test.method,
      headers: test.headers,
      withCred: test.withCred,
    };

    if (test.allowCred) {
      req.url += "&allowCred";
    }

    if (test.setCookie) {
      req.url += "&setCookie=" + escape(test.setCookie);
    }
    if (test.cookie) {
      req.url += "&cookie=" + escape(test.cookie);
    }
    if (test.noCookie) {
      req.url += "&noCookie";
    }

    if ("allowHeaders" in test) {
      req.url += "&allowHeaders=" + escape(test.allowHeaders);
    }
    if ("allowMethods" in test) {
      req.url += "&allowMethods=" + escape(test.allowMethods);
    }

    return new Request(req.url, {
      method: req.method,
      headers: req.headers,
      credentials: req.withCred,
    });
  }

  function testResponse(res, test) {
    ok(test.pass, "Expected test to pass for " + JSON.stringify(test));
    is(res.status, 200, "wrong status in test for " + JSON.stringify(test));
    is(res.statusText, "OK", "wrong status text for " + JSON.stringify(test));
    return res.text().then(function(v) {
      is(
        v,
        "<res>hello pass</res>\n",
        "wrong text in test for " + JSON.stringify(test)
      );
    });
  }

  function runATest(tests, i) {
    var test = tests[i];
    var request = makeRequest(test);
    fetch(request).then(
      function(res) {
        testResponse(res, test).then(function() {
          if (i < tests.length - 1) {
            runATest(tests, i + 1);
          } else {
            finalPromiseResolve();
          }
        });
      },
      function(e) {
        ok(!test.pass, "Expected test failure for " + JSON.stringify(test));
        ok(
          e instanceof TypeError,
          "Exception should be TypeError for " + JSON.stringify(test)
        );
        if (i < tests.length - 1) {
          runATest(tests, i + 1);
        } else {
          finalPromiseResolve();
        }
      }
    );
  }

  runATest(tests, 0);
  return finalPromise;
}

function testModeNoCorsCredentials() {
  var cookieStr = "type=chocolatechip";
  var tests = [
    {
      // Initialize by setting a cookie.
      pass: 1,
      setCookie: cookieStr,
      withCred: "include",
    },
    {
      pass: 1,
      noCookie: 1,
      withCred: "omit",
    },
    {
      pass: 1,
      noCookie: 1,
      withCred: "same-origin",
    },
    {
      pass: 1,
      cookie: cookieStr,
      withCred: "include",
    },
    {
      pass: 1,
      cookie: cookieStr,
      withCred: "omit",
      status: 500,
    },
    {
      pass: 1,
      cookie: cookieStr,
      withCred: "same-origin",
      status: 500,
    },
    {
      pass: 1,
      noCookie: 1,
      withCred: "include",
      status: 500,
    },
  ];

  var finalPromiseResolve, finalPromiseReject;
  var finalPromise = new Promise(function(res, rej) {
    finalPromiseResolve = res;
    finalPromiseReject = rej;
  });

  function makeRequest(test) {
    req = {
      url: "http://example.org" + corsServerPath + "a+b",
      withCred: test.withCred,
    };

    if (test.setCookie) {
      req.url += "&setCookie=" + escape(test.setCookie);
    }
    if (test.cookie) {
      req.url += "&cookie=" + escape(test.cookie);
    }
    if (test.noCookie) {
      req.url += "&noCookie";
    }

    return new Request(req.url, {
      method: "GET",
      mode: "no-cors",
      credentials: req.withCred,
    });
  }

  function testResponse(res, test) {
    is(res.type, "opaque", "wrong response type for " + JSON.stringify(test));

    // Get unfiltered response
    var chromeResponse = SpecialPowers.wrap(res);
    var unfiltered = chromeResponse.cloneUnfiltered();

    var status = test.status ? test.status : 200;
    is(
      unfiltered.status,
      status,
      "wrong status in test for " + JSON.stringify(test)
    );
    return unfiltered.text().then(function(v) {
      if (status === 200) {
        is(
          v,
          "<res>hello pass</res>\n",
          "wrong text in test for " + JSON.stringify(test)
        );
      }
    });
  }

  function runATest(tests, i) {
    if (typeof SpecialPowers !== "object") {
      finalPromiseResolve();
      return;
    }

    var test = tests[i];
    var request = makeRequest(test);
    fetch(request).then(
      function(res) {
        testResponse(res, test).then(function() {
          if (i < tests.length - 1) {
            runATest(tests, i + 1);
          } else {
            finalPromiseResolve();
          }
        });
      },
      function(e) {
        ok(!test.pass, "Expected test to fail " + JSON.stringify(test));
        ok(e instanceof TypeError, "Test should fail " + JSON.stringify(test));
        if (i < tests.length - 1) {
          runATest(tests, i + 1);
        } else {
          finalPromiseResolve();
        }
      }
    );
  }

  runATest(tests, 0);
  return finalPromise;
}

function testCORSRedirects() {
  var origin = "http://mochi.test:8888";

  var tests = [
    {
      pass: 1,
      method: "GET",
      hops: [{ server: "http://example.com", allowOrigin: origin }],
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://mochi.test:8888", allowOrigin: origin },
      ],
    },
    {
      pass: 1,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://mochi.test:8888", allowOrigin: "*" },
      ],
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://mochi.test:8888" },
      ],
    },
    {
      pass: 1,
      method: "GET",
      hops: [
        { server: "http://mochi.test:8888" },
        { server: "http://mochi.test:8888" },
        { server: "http://example.com", allowOrigin: origin },
      ],
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: "http://mochi.test:8888" },
        { server: "http://mochi.test:8888" },
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://mochi.test:8888" },
      ],
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://test2.mochi.test:8888", allowOrigin: origin },
        {
          server: "http://sub2.xn--lt-uia.mochi.test:8888",
          allowOrigin: origin,
        },
        { server: "http://sub1.test1.mochi.test:8888", allowOrigin: origin },
      ],
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://test2.mochi.test:8888", allowOrigin: origin },
        { server: "http://sub2.xn--lt-uia.mochi.test:8888", allowOrigin: "*" },
        { server: "http://sub1.test1.mochi.test:8888", allowOrigin: "*" },
      ],
    },
    {
      pass: 1,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://test2.mochi.test:8888", allowOrigin: "*" },
        { server: "http://sub2.xn--lt-uia.mochi.test:8888", allowOrigin: "*" },
        { server: "http://sub1.test1.mochi.test:8888", allowOrigin: "*" },
      ],
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://test2.mochi.test:8888", allowOrigin: origin },
        { server: "http://sub2.xn--lt-uia.mochi.test:8888", allowOrigin: "x" },
        { server: "http://sub1.test1.mochi.test:8888", allowOrigin: origin },
      ],
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://test2.mochi.test:8888", allowOrigin: origin },
        { server: "http://sub2.xn--lt-uia.mochi.test:8888", allowOrigin: "*" },
        { server: "http://sub1.test1.mochi.test:8888", allowOrigin: origin },
      ],
    },
    {
      pass: 0,
      method: "GET",
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://test2.mochi.test:8888", allowOrigin: origin },
        { server: "http://sub2.xn--lt-uia.mochi.test:8888", allowOrigin: "*" },
        { server: "http://sub1.test1.mochi.test:8888" },
      ],
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain" },
      hops: [
        { server: "http://mochi.test:8888" },
        { server: "http://example.com", allowOrigin: origin },
      ],
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain", "my-header": "myValue" },
      hops: [
        { server: "http://mochi.test:8888" },
        {
          server: "http://example.com",
          allowOrigin: origin,
          allowHeaders: "my-header",
        },
      ],
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain", "my-header": "myValue" },
      hops: [
        { server: "http://mochi.test:8888" },
        {
          server: "http://example.com",
          allowOrigin: origin,
          allowHeaders: "my-header",
          noAllowPreflight: 1,
        },
      ],
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain", "my-header": "myValue" },
      hops: [
        { server: "http://mochi.test:8888" },
        {
          server: "http://test1.example.com",
          allowOrigin: origin,
          allowHeaders: "my-header",
        },
        {
          server: "http://test2.example.com",
          allowOrigin: origin,
          allowHeaders: "my-header",
        },
      ],
    },
    {
      pass: 1,
      method: "DELETE",
      hops: [
        { server: "http://mochi.test:8888" },
        {
          server: "http://example.com",
          allowOrigin: origin,
          allowMethods: "DELETE",
        },
      ],
    },
    {
      pass: 0,
      method: "DELETE",
      hops: [
        { server: "http://mochi.test:8888" },
        {
          server: "http://example.com",
          allowOrigin: origin,
          allowMethods: "DELETE",
          noAllowPreflight: 1,
        },
      ],
    },
    {
      pass: 0,
      method: "DELETE",
      hops: [
        { server: "http://mochi.test:8888" },
        {
          server: "http://test1.example.com",
          allowOrigin: origin,
          allowMethods: "DELETE",
        },
        {
          server: "http://test2.example.com",
          allowOrigin: origin,
          allowMethods: "DELETE",
        },
      ],
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain", "my-header": "myValue" },
      hops: [
        { server: "http://example.com", allowOrigin: origin },
        { server: "http://sub1.test1.mochi.test:8888", allowOrigin: origin },
      ],
    },
    {
      pass: 0,
      method: "DELETE",
      hops: [
        {
          server: "http://example.com",
          allowOrigin: origin,
          allowMethods: "DELETE",
        },
        {
          server: "http://sub1.test1.mochi.test:8888",
          allowOrigin: origin,
          allowMethods: "DELETE",
        },
      ],
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain", "my-header": "myValue" },
      hops: [
        { server: "http://example.com" },
        {
          server: "http://sub1.test1.mochi.test:8888",
          allowOrigin: origin,
          allowHeaders: "my-header",
        },
      ],
    },
    {
      pass: 1,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain" },
      hops: [
        { server: "http://mochi.test:8888" },
        { server: "http://example.com", allowOrigin: origin },
      ],
    },
    {
      pass: 0,
      method: "POST",
      body: "hi there",
      headers: { "Content-Type": "text/plain", "my-header": "myValue" },
      hops: [
        {
          server: "http://example.com",
          allowOrigin: origin,
          allowHeaders: "my-header",
        },
        {
          server: "http://mochi.test:8888",
          allowOrigin: origin,
          allowHeaders: "my-header",
        },
      ],
    },
  ];

  var fetches = [];
  for (test of tests) {
    req = {
      url:
        test.hops[0].server +
        corsServerPath +
        "hop=1&hops=" +
        escape(JSON.stringify(test.hops)),
      method: test.method,
      headers: test.headers,
      body: test.body,
    };

    if (test.headers) {
      req.url += "&headers=" + escape(JSON.stringify(test.headers));
    }

    if (test.pass) {
      if (test.body) {
        req.url += "&body=" + escape(test.body);
      }
    }

    var request = new Request(req.url, {
      method: req.method,
      headers: req.headers,
      body: req.body,
    });
    fetches.push(
      (function(request, test) {
        return fetch(request).then(
          function(res) {
            ok(test.pass, "Expected test to pass for " + JSON.stringify(test));
            is(
              res.status,
              200,
              "wrong status in test for " + JSON.stringify(test)
            );
            is(
              res.statusText,
              "OK",
              "wrong status text for " + JSON.stringify(test)
            );
            is(
              res.type,
              "cors",
              "wrong response type for " + JSON.stringify(test)
            );
            var reqHost = new URL(req.url).host;
            // If there is a service worker present, the redirections will be
            // transparent, assuming that the original request is to the current
            // site and would be intercepted.
            if (isSWPresent) {
              if (reqHost === location.host) {
                is(
                  new URL(res.url).host,
                  reqHost,
                  "Response URL should be original URL with a SW present"
                );
              }
            } else {
              is(
                new URL(res.url).host,
                new URL(test.hops[test.hops.length - 1].server).host,
                "Response URL should be redirected URL"
              );
            }
            return res.text().then(function(v) {
              is(
                v,
                "<res>hello pass</res>\n",
                "wrong responseText in test for " + JSON.stringify(test)
              );
            });
          },
          function(e) {
            ok(!test.pass, "Expected test failure for " + JSON.stringify(test));
            ok(
              e instanceof TypeError,
              "Exception should be TypeError for " + JSON.stringify(test)
            );
          }
        );
      })(request, test)
    );
  }

  return Promise.all(fetches);
}

function testNoCORSRedirects() {
  var origin = "http://mochi.test:8888";

  var tests = [
    { pass: 1, method: "GET", hops: [{ server: "http://example.com" }] },
    {
      pass: 1,
      method: "GET",
      hops: [{ server: origin }, { server: "http://example.com" }],
    },
    {
      pass: 1,
      method: "GET",
      // Must use a simple header due to no-cors header restrictions.
      headers: { "accept-language": "en-us" },
      hops: [{ server: origin }, { server: "http://example.com" }],
    },
    {
      pass: 1,
      method: "GET",
      hops: [
        { server: origin },
        { server: "http://example.com" },
        { server: origin },
      ],
    },
    {
      pass: 1,
      method: "POST",
      body: "upload body here",
      hops: [{ server: origin }, { server: "http://example.com" }],
    },
    {
      pass: 0,
      method: "DELETE",
      hops: [{ server: origin }, { server: "http://example.com" }],
    },
  ];

  var fetches = [];
  for (test of tests) {
    req = {
      url:
        test.hops[0].server +
        corsServerPath +
        "hop=1&hops=" +
        escape(JSON.stringify(test.hops)),
      method: test.method,
      headers: test.headers,
      body: test.body,
    };

    if (test.headers) {
      req.url += "&headers=" + escape(JSON.stringify(test.headers));
    }

    if (test.pass) {
      if (test.body) {
        req.url += "&body=" + escape(test.body);
      }
    }

    fetches.push(
      (function(req, test) {
        return new Promise(function(resolve, reject) {
          resolve(
            new Request(req.url, {
              mode: "no-cors",
              method: req.method,
              headers: req.headers,
              body: req.body,
            })
          );
        })
          .then(function(request) {
            return fetch(request);
          })
          .then(
            function(res) {
              ok(
                test.pass,
                "Expected test to pass for " + JSON.stringify(test)
              );
              // All requests are cross-origin no-cors, we should always have
              // an opaque response here.  All values on the opaque response
              // should be hidden.
              is(
                res.type,
                "opaque",
                "wrong response type for " + JSON.stringify(test)
              );
              is(
                res.status,
                0,
                "wrong status in test for " + JSON.stringify(test)
              );
              is(
                res.statusText,
                "",
                "wrong status text for " + JSON.stringify(test)
              );
              is(res.url, "", "wrong response url for " + JSON.stringify(test));
              return res.text().then(function(v) {
                is(
                  v,
                  "",
                  "wrong responseText in test for " + JSON.stringify(test)
                );
              });
            },
            function(e) {
              ok(
                !test.pass,
                "Expected test failure for " + JSON.stringify(test)
              );
              ok(
                e instanceof TypeError,
                "Exception should be TypeError for " + JSON.stringify(test)
              );
            }
          );
      })(req, test)
    );
  }

  return Promise.all(fetches);
}

function testReferrer() {
  var referrer;
  if (self && self.location) {
    referrer = self.location.href;
  } else {
    referrer = document.documentURI;
  }

  var dict = {
    Referer: referrer,
  };
  return fetch(
    corsServerPath + "headers=" + encodeURIComponent(JSON.stringify(dict))
  ).then(
    function(res) {
      is(res.status, 200, "expected correct referrer header to be sent");
      dump(res.statusText);
    },
    function(e) {
      ok(false, "expected correct referrer header to be sent");
    }
  );
}

function runTest() {
  testNoCorsCtor();

  return Promise.resolve()
    .then(testModeSameOrigin)
    .then(testModeNoCors)
    .then(testModeCors)
    .then(testSameOriginCredentials)
    .then(testCrossOriginCredentials)
    .then(testModeNoCorsCredentials)
    .then(testCORSRedirects)
    .then(testNoCORSRedirects)
    .then(testReferrer);
  // Put more promise based tests here.
}
