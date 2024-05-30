/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests loading sourcemapped sources, setting breakpoints, and
// stepping in them.

"use strict";

const EXPECTED_COOKIE = "credentials=true";

// This test checks that the DevTools sourcemap request includes credentials
// from the page. See Bug 1899389.
add_task(async function () {
  const httpServer = setupTestServer();
  const port = httpServer.identity.primaryPort;

  const dbg = await initDebuggerWithAbsoluteURL(
    `http://localhost:${port}/index.html`
  );

  await waitForSources(dbg, "min.js");
  const minifiedSrc = findSource(dbg, "min.js");
  await selectSource(dbg, minifiedSrc);

  const footerButton = findElement(dbg, "sourceMapFooterButton");
  is(
    footerButton.textContent,
    "bundle file",
    "Sourcemap button mentions the bundle file"
  );
  ok(!footerButton.classList.contains("not-mapped"));

  info("Click on jump to original source link from editor's footer");
  const mappedSourceLink = findElement(dbg, "mappedSourceLink");
  is(
    mappedSourceLink.textContent,
    "From original.js",
    "The link to mapped source mentions the original file name"
  );
  mappedSourceLink.click();

  const originalSrc = findSource(dbg, "original.js");
  await waitForSelectedSource(dbg, originalSrc);
  info("The original source was successfully selected");
});

function setupTestServer() {
  // The test server will serve:
  // - index.html: page which loads a min.js file
  // - min.js: simple minified js file, linked to the map min.js.map
  // - min.js.map: the corresponding sourcemap
  // - original.js: the original file
  //
  // The sourcemap file will only be returned if the request contains a cookie
  // set in the page. Otherwise it will return a 404.

  const httpServer = createTestHTTPServer();
  httpServer.registerContentType("html", "text/html");
  httpServer.registerContentType("js", "application/javascript");
  httpServer.registerContentType("map", "text/plain");

  // Page: index.html
  httpServer.registerPathHandler("/index.html", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`<!DOCTYPE html>
    <html>
      <body>
      <h1>Sourcemap with credentials</h1>
      <script src="/min.js"></script>
      <script type=text/javascript>document.cookie="${EXPECTED_COOKIE}";</script>
      </body>
    `);
  });

  // Bundle: min.js
  httpServer.registerPathHandler("/min.js", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/javascript", false);
    response.write(`function sum(n,u){return n+u}
//# sourceMappingURL=min.js.map
`);
  });

  // Sourcemap: min.js.map
  httpServer.registerPathHandler("/min.js.map", function (request, response) {
    if (
      request.hasHeader("Cookie") &&
      request.getHeader("Cookie") == EXPECTED_COOKIE
    ) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/javascript", false);
      response.write(
        `{"version":3,"sources":["original.js"],"names":["sum","first","second"],"mappings":"AAAA,SAASA,IAAIC,EAAOC,GAClB,OAAOD,EAAQC"}`
      );
    } else {
      response.setStatusLine(request.httpVersion, 404, "Not found");
    }
  });

  // Original: original.js
  httpServer.registerPathHandler("/original.js", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "application/javascript", false);
    response.write(`function sum(first, second) {
  return first + second;
}`);
  });

  return httpServer;
}
