/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test the redirects on the sourceMappingURL are blocked and not followed.

"use strict";

const httpServer = createTestHTTPServer();
const BASE_URL = `http://localhost:${httpServer.identity.primaryPort}`;

httpServer.registerContentType("html", "text/html");
httpServer.registerContentType("js", "application/javascript");

httpServer.registerPathHandler("/index.html", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`<!doctype html>
    <html>
      <head>
        <script>
          const foo = 2;
          console.log(foo);
          //# sourceMappingURL=${BASE_URL}/redirect
        </script>
      </head>
    </html>
  `);
});

httpServer.registerPathHandler("/redirect", (request, response) => {
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", `${BASE_URL}/evil`);
});

httpServer.registerPathHandler("/evil", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(
    `{"version":3,"sources":["evil.original.js"],"names":[], "mappings": ""}`
  );
});

add_task(async function () {
  const dbg = await initDebuggerWithAbsoluteURL(`${BASE_URL}/index.html`);

  await getDebuggerSplitConsole(dbg);
  await hasConsoleMessage(dbg, "Source map error");
  const { value } = await findConsoleMessage(dbg, "Source map error");

  is(
    value,
    `Source map error: NetworkError when attempting to fetch resource.\nResource URL: ${BASE_URL}/index.html\nSource Map URL: ${BASE_URL}/redirect[Learn More]`,
    "A source map error message is logged indicating the redirect failed"
  );
});
