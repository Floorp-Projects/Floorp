/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.fetch on http:// URI's.

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const server = new HttpServer();
server.registerDirectory("/", do_get_cwd());
server.registerPathHandler("/cached.json", cacheRequestHandler);
server.start(-1);

const port = server.identity.primaryPort;
const serverURL = "http://localhost:" + port;
const CACHED_URL = serverURL + "/cached.json";
const NORMAL_URL = serverURL + "/test_fetch-http.js";

function cacheRequestHandler(request, response) {
  info("Got request for " + request.path);
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);

  const body = "[" + Math.random() + "]";
  response.bodyOutputStream.write(body, body.length);
}

do_get_profile();

registerCleanupFunction(() => {
  return new Promise(resolve => server.stop(resolve));
});

add_task(async function test_normal() {
  await DevToolsUtils.fetch(NORMAL_URL).then(({ content }) => {
    ok(
      content.includes("The content looks correct."),
      "The content looks correct."
    );
  });
});

add_task(async function test_caching() {
  let initialContent = null;

  info("Performing the first request.");
  await DevToolsUtils.fetch(CACHED_URL).then(({ content }) => {
    info("Got the first response: " + content);
    initialContent = content;
  });

  info("Performing another request, expecting to get cached response.");
  await DevToolsUtils.fetch(CACHED_URL).then(({ content }) => {
    deepEqual(content, initialContent, "The content was loaded from cache.");
  });

  info("Performing a third request with cache bypassed.");
  const opts = { loadFromCache: false };
  await DevToolsUtils.fetch(CACHED_URL, opts).then(({ content }) => {
    notDeepEqual(
      content,
      initialContent,
      "The URL wasn't loaded from cache with loadFromCache: false."
    );
  });
});
