"use strict";
// Cu is defined in xpc_head.js
/* globals Cu */

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js"); /* globals HttpServer */
Cu.import("resource://gre/modules/osfile.jsm", this); /* globals OS */
Cu.import("resource://shield-recipe-client/lib/NormandyApi.jsm", this);

load("utils.js"); /* globals withMockPreferences */

function withServer(server, task) {
  return withMockPreferences(async function inner(preferences) {
    const serverUrl = `http://localhost:${server.identity.primaryPort}`;
    preferences.set("extensions.shield-recipe-client.api_url", `${serverUrl}/api/v1`);
    preferences.set(
      "security.content.signature.root_hash",
      // Hash of the key that signs the normandy dev certificates
      "4C:35:B1:C3:E3:12:D9:55:E7:78:ED:D0:A7:E7:8A:38:83:04:EF:01:BF:FA:03:29:B2:46:9F:3C:C5:EC:36:04"
    );

    try {
      await task(serverUrl);
    } finally {
      await new Promise(resolve => server.stop(resolve));
    }
  });
}

function makeScriptServer(scriptPath) {
  const server = new HttpServer();
  server.registerContentType("sjs", "sjs");
  server.registerFile("/", do_get_file(scriptPath));
  server.start(-1);
  return server;
}

function withScriptServer(scriptPath, task) {
  return withServer(makeScriptServer(scriptPath), task);
}

function makeMockApiServer() {
  const server = new HttpServer();
  server.registerDirectory("/", do_get_file("mock_api"));

  server.setIndexHandler(async function(request, response) {
    response.processAsync();
    const dir = request.getProperty("directory");
    const index = dir.clone();
    index.append("index.json");

    if (!index.exists()) {
      response.setStatusLine("1.1", 404, "Not Found");
      response.write(`Cannot find path ${index.path}`);
      response.finish();
      return;
    }

    try {
      const contents = await OS.File.read(index.path, {encoding: "utf-8"});
      response.write(contents);
    } catch (e) {
      response.setStatusLine("1.1", 500, "Server error");
      response.write(e.toString());
    } finally {
      response.finish();
    }
  });

  server.start(-1);
  return server;
}

function withMockApiServer(task) {
  return withServer(makeMockApiServer(), task);
}

add_task(withMockApiServer(async function test_get(serverUrl) {
  // Test that NormandyApi can fetch from the test server.
  const response = await NormandyApi.get(`${serverUrl}/api/v1`);
  const data = await response.json();
  equal(data["recipe-list"], "/api/v1/recipe/", "Expected data in response");
}));

add_task(withMockApiServer(async function test_getApiUrl(serverUrl) {
  const apiBase = `${serverUrl}/api/v1`;
  // Test that NormandyApi can use the self-describing API's index
  const recipeListUrl = await NormandyApi.getApiUrl("action-list");
  equal(recipeListUrl, `${apiBase}/action/`, "Can retrieve action-list URL from API");
}));

add_task(withMockApiServer(async function test_fetchRecipes() {
  const recipes = await NormandyApi.fetchRecipes();
  equal(recipes.length, 1);
  equal(recipes[0].name, "system-addon-test");
}));

add_task(withMockApiServer(async function test_classifyClient() {
  const classification = await NormandyApi.classifyClient();
  Assert.deepEqual(classification, {
    country: "US",
    request_time: new Date("2017-02-22T17:43:24.657841Z"),
  });
}));

add_task(withMockApiServer(async function test_fetchActions() {
  const actions = await NormandyApi.fetchActions();
  equal(actions.length, 2);
  const actionNames = actions.map(a => a.name);
  ok(actionNames.includes("console-log"));
  ok(actionNames.includes("show-heartbeat"));
}));

add_task(withScriptServer("query_server.sjs", async function test_getTestServer(serverUrl) {
  // Test that NormandyApi can fetch from the test server.
  const response = await NormandyApi.get(serverUrl);
  const data = await response.json();
  Assert.deepEqual(data, {queryString: {}, body: {}}, "NormandyApi returned incorrect server data.");
}));

add_task(withScriptServer("query_server.sjs", async function test_getQueryString(serverUrl) {
  // Test that NormandyApi can send query string parameters to the test server.
  const response = await NormandyApi.get(serverUrl, {foo: "bar", baz: "biff"});
  const data = await response.json();
  Assert.deepEqual(
    data, {queryString: {foo: "bar", baz: "biff"}, body: {}},
    "NormandyApi sent an incorrect query string."
  );
}));

add_task(withScriptServer("query_server.sjs", async function test_postData(serverUrl) {
  // Test that NormandyApi can POST JSON-formatted data to the test server.
  const response = await NormandyApi.post(serverUrl, {foo: "bar", baz: "biff"});
  const data = await response.json();
  Assert.deepEqual(
    data, {queryString: {}, body: {foo: "bar", baz: "biff"}},
    "NormandyApi sent an incorrect query string."
  );
}));

add_task(withScriptServer("echo_server.sjs", async function test_fetchImplementation(serverUrl) {
  const action = {
    implementation_url: `${serverUrl}?status=200&body=testcontent`,
  };
  equal(
    await NormandyApi.fetchImplementation(action),
    "testcontent",
    "fetchImplementation fetches the content at the correct URL",
  );
}));

add_task(withScriptServer(
  "echo_server.sjs",
  async function test_fetchImplementationFail(serverUrl) {
    const action = {
      implementation_url: `${serverUrl}?status=500&body=servererror`,
    };

    try {
      await NormandyApi.fetchImplementation(action);
      ok(false, "fetchImplementation throws for non-200 response status codes");
    } catch (err) {
      // pass
    }
  },
));
