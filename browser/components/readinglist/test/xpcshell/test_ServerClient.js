/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource:///modules/readinglist/ServerClient.jsm");
Cu.import("resource://gre/modules/Log.jsm");

let appender = new Log.DumpAppender();
for (let logName of ["FirefoxAccounts", "readinglist.serverclient"]) {
  Log.repository.getLogger(logName).addAppender(appender);
}

// Some test servers we use.
let Server = function(handlers) {
  this._server = null;
  this._handlers = handlers;
}

Server.prototype = {
  start() {
    this._server = new HttpServer();
    for (let [path, handler] in Iterator(this._handlers)) {
      // httpd.js seems to swallow exceptions
      let thisHandler = handler;
      let wrapper = (request, response) => {
        try {
          thisHandler(request, response);
        } catch (ex) {
          print("**** Handler for", path, "failed:", ex, ex.stack);
          throw ex;
        }
      }
      this._server.registerPathHandler(path, wrapper);
    }
    this._server.start(-1);
  },

  stop() {
    return new Promise(resolve => {
      this._server.stop(resolve);
      this._server = null;
    });
  },

  get host() {
    return "http://localhost:" + this._server.identity.primaryPort;
  },
};

// An OAuth server that hands out tokens.
function OAuthTokenServer() {
  let server;
  let handlers = {
    "/v1/authorization": (request, response) => {
      response.setStatusLine("1.1", 200, "OK");
      let token = "token" + server.numTokenFetches;
      print("Test OAuth server handing out token", token);
      server.numTokenFetches += 1;
      server.activeTokens.add(token);
      response.write(JSON.stringify({access_token: token}));
    },
    "/v1/destroy": (request, response) => {
      // Getting the body seems harder than it should be!
      let sis = Cc["@mozilla.org/scriptableinputstream;1"]
                .createInstance(Ci.nsIScriptableInputStream);
      sis.init(request.bodyInputStream);
      let body = JSON.parse(sis.read(sis.available()));
      sis.close();
      let token = body.token;
      ok(server.activeTokens.delete(token));
      print("after destroy have", server.activeTokens.size, "tokens left.")
      response.setStatusLine("1.1", 200, "OK");
      response.write('{}');
    },
  }
  server = new Server(handlers);
  server.numTokenFetches = 0;
  server.activeTokens = new Set();
  return server;
}

function promiseObserver(topic) {
  return new Promise(resolve => {
    function observe(subject, topic, data) {
      Services.obs.removeObserver(observe, topic);
      resolve(data);
    }
    Services.obs.addObserver(observe, topic, false);
  });
}

// The tests.
function run_test() {
  run_next_test();
}

// Arrange for the first token we hand out to be rejected - the client should
// notice the 401 and silently get a new token and retry the request.
add_task(function testAuthRetry() {
  let handlers = {
    "/v1/batch": (request, response) => {
      // We know the first token we will get is "token0", so we simulate that
      // "expiring" by only accepting "token1". Then we just echo the response
      // back.
      let authHeader;
      try {
        authHeader = request.getHeader("Authorization");
      } catch (ex) {}
      if (authHeader != "Bearer token1") {
        response.setStatusLine("1.1", 401, "Unauthorized");
        response.write("wrong token");
        return;
      }
      response.setStatusLine("1.1", 200, "OK");
      response.write(JSON.stringify({ok: true}));
    }
  };
  let rlserver = new Server(handlers);
  rlserver.start();
  let authServer = OAuthTokenServer();
  authServer.start();
  try {
    Services.prefs.setCharPref("readinglist.server", rlserver.host + "/v1");
    Services.prefs.setCharPref("identity.fxaccounts.remote.oauth.uri", authServer.host + "/v1");

    let fxa = yield createMockFxA();
    let sc = new ServerClient(fxa);

    let response = yield sc.request({
      path: "/batch",
      method: "post",
      body: {foo: "bar"},
    });
    equal(response.status, 200, "got the 200 we expected");
    equal(authServer.numTokenFetches, 2, "took 2 tokens to get the 200")
    deepEqual(response.body, {ok: true});
  } finally {
    yield authServer.stop();
    yield rlserver.stop();
  }
});

// Check that specified headers are seen by the server, and that server headers
// in the response are seen by the client.
add_task(function testHeaders() {
  let handlers = {
    "/v1/batch": (request, response) => {
      ok(request.hasHeader("x-foo"), "got our foo header");
      equal(request.getHeader("x-foo"), "bar", "foo header has the correct value");
      response.setHeader("Server-Sent-Header", "hello");
      response.setStatusLine("1.1", 200, "OK");
      response.write("{}");
    }
  };
  let rlserver = new Server(handlers);
  rlserver.start();
  try {
    Services.prefs.setCharPref("readinglist.server", rlserver.host + "/v1");

    let fxa = yield createMockFxA();
    let sc = new ServerClient(fxa);
    sc._getToken = () => Promise.resolve();

    let response = yield sc.request({
      path: "/batch",
      method: "post",
      headers: {"X-Foo": "bar"},
      body: {foo: "bar"}});
    equal(response.status, 200, "got the 200 we expected");
    equal(response.headers["server-sent-header"], "hello", "got the server header");
  } finally {
    yield rlserver.stop();
  }
});

// Check that a "backoff" header causes the correct notification.
add_task(function testBackoffHeader() {
  let handlers = {
    "/v1/batch": (request, response) => {
      response.setHeader("Backoff", "123");
      response.setStatusLine("1.1", 200, "OK");
      response.write("{}");
    }
  };
  let rlserver = new Server(handlers);
  rlserver.start();

  let observerPromise = promiseObserver("readinglist:backoff-requested");
  try {
    Services.prefs.setCharPref("readinglist.server", rlserver.host + "/v1");

    let fxa = yield createMockFxA();
    let sc = new ServerClient(fxa);
    sc._getToken = () => Promise.resolve();

    let response = yield sc.request({
      path: "/batch",
      method: "post",
      headers: {"X-Foo": "bar"},
      body: {foo: "bar"}});
    equal(response.status, 200, "got the 200 we expected");
    let data = yield observerPromise;
    equal(data, "123", "got the expected header value.")
  } finally {
    yield rlserver.stop();
  }
});

// Check that a "backoff" header causes the correct notification.
add_task(function testRetryAfterHeader() {
  let handlers = {
    "/v1/batch": (request, response) => {
      response.setHeader("Retry-After", "456");
      response.setStatusLine("1.1", 500, "Not OK");
      response.write("{}");
    }
  };
  let rlserver = new Server(handlers);
  rlserver.start();

  let observerPromise = promiseObserver("readinglist:backoff-requested");
  try {
    Services.prefs.setCharPref("readinglist.server", rlserver.host + "/v1");

    let fxa = yield createMockFxA();
    let sc = new ServerClient(fxa);
    sc._getToken = () => Promise.resolve();

    let response = yield sc.request({
      path: "/batch",
      method: "post",
      headers: {"X-Foo": "bar"},
      body: {foo: "bar"}});
    equal(response.status, 500, "got the 500 we expected");
    let data = yield observerPromise;
    equal(data, "456", "got the expected header value.")
  } finally {
    yield rlserver.stop();
  }
});

// Check that unicode ends up as utf-8 in requests, and vice-versa in responses.
// (Note the ServerClient assumes all strings in and out are UCS, and thus have
// already been encoded/decoded (ie, it never expects to receive stuff already
// utf-8 encoded, and never returns utf-8 encoded responses.)
add_task(function testUTF8() {
  let handlers = {
    "/v1/hello": (request, response) => {
      // Get the body as bytes.
      let sis = Cc["@mozilla.org/scriptableinputstream;1"]
                .createInstance(Ci.nsIScriptableInputStream);
      sis.init(request.bodyInputStream);
      let body = sis.read(sis.available());
      sis.close();
      // The client sent "{"copyright: "\xa9"} where \xa9 is the copyright symbol.
      // It should have been encoded as utf-8 which is \xc2\xa9
      equal(body, '{"copyright":"\xc2\xa9"}', "server saw utf-8 encoded data");
      // and just write it back unchanged.
      response.setStatusLine("1.1", 200, "OK");
      response.write(body);
    }
  };
  let rlserver = new Server(handlers);
  rlserver.start();
  try {
    Services.prefs.setCharPref("readinglist.server", rlserver.host + "/v1");

    let fxa = yield createMockFxA();
    let sc = new ServerClient(fxa);
    sc._getToken = () => Promise.resolve();

    let body = {copyright: "\xa9"}; // see above - \xa9 is the copyright symbol
    let response = yield sc.request({
      path: "/hello",
      method: "post",
      body: body
    });
    equal(response.status, 200, "got the 200 we expected");
    deepEqual(response.body, body);
  } finally {
    yield rlserver.stop();
  }
});
