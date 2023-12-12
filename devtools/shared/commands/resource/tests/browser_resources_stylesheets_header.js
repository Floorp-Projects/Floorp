/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that we do get the appropriate stylesheet content when the stylesheet is only
// served based on the Accept: text/css header

add_task(async function () {
  const httpServer = createTestHTTPServer();

  httpServer.registerContentType("html", "text/html");

  httpServer.registerPathHandler("/index.html", function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(`
<!DOCTYPE html>
<meta charset="utf-8">
<title>Test stylesheet</title>
<link href="/test/" rel="stylesheet" type="text/css"/>
<script src="/test/"></script>
<h1>Hello</h1>
  `);
  });

  let resourceUrlCalls = 0;
  // The /test/ URL should be called:
  // - once by the content page to load the <link>
  // - once by the content page to load the <script>
  // - once by DevTools to fetch the stylesheet text
  // (we could probably optimize this so we only call once)
  const expectedResourceUrlCalls = 3;

  const styleSheetText = `body { background-color: tomato; }`;
  httpServer.registerPathHandler("/test/", function (request, response) {
    resourceUrlCalls++;
    response.setStatusLine(request.httpVersion, 200, "OK");

    if (request.getHeader("Accept").startsWith("text/css")) {
      response.setHeader("Content-Type", "text/css", false);
      response.write(styleSheetText);
      return;
    }
    response.setHeader("Content-Type", "application/javascript", false);
    response.write(`/* NOT A STYLESHEET */`);
  });
  const port = httpServer.identity.primaryPort;
  const TEST_URL = `http://localhost:${port}/index.html`;

  info("Check resource available feature of the ResourceCommand");
  const tab = await addTab(TEST_URL);

  const { client, resourceCommand, targetCommand } = await initResourceCommand(
    tab
  );

  info("Check whether ResourceCommand gets existing stylesheet");
  const availableResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.STYLESHEET], {
    onAvailable: resources => availableResources.push(...resources),
  });
  is(
    availableResources.length,
    1,
    "We have the expected number of stylesheets"
  );

  is(
    await getStyleSheetResourceText(availableResources[0]),
    styleSheetText,
    "Got expected text for the stylesheet"
  );

  is(
    resourceUrlCalls,
    expectedResourceUrlCalls,
    "The /test URL was called the number of time we expected"
  );

  targetCommand.destroy();
  await client.close();
});
