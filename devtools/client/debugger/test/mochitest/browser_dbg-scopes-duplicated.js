/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that properties with the same value objec be expanded. See Bug 1617210.

const httpServer = createTestHTTPServer();
httpServer.registerContentType("html", "text/html");
httpServer.registerContentType("js", "application/javascript");

httpServer.registerPathHandler(`/`, function(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(`
      <html>
          <button class="pause">Click me</button>
          <script type="text/javascript" src="test.js"></script>
      </html>`);
});

httpServer.registerPathHandler("/test.js", function(request, response) {
  response.setHeader("Content-Type", "application/javascript");
  response.write(`
    document.addEventListener("click", function onClick(e) {
      var hello = {hello: "world"};
      var x = {a: hello, b: hello, c: hello};
      debugger;
    });
  `);
});
const port = httpServer.identity.primaryPort;
const TEST_URL = `http://localhost:${port}/`;

add_task(async function() {
  const dbg = await initDebuggerWithAbsoluteURL(TEST_URL);

  const ready = Promise.all([
    waitForPaused(dbg),
    waitForLoadedSource(dbg, "test"),
  ]);

  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.document.querySelector("button.pause").click();
  });

  await ready;

  is(getLabel(dbg, 4), "e");
  is(getLabel(dbg, 5), "hello");
  is(getLabel(dbg, 6), "x");

  info("Expand `x` node");
  await toggleScopeNode(dbg, 6);
  is(getLabel(dbg, 7), "a");
  is(getLabel(dbg, 8), "b");
  is(getLabel(dbg, 9), "c");

  info("Expand `c`, `b` and `a` nodes");
  await toggleScopeNode(dbg, 9);
  await toggleScopeNode(dbg, 8);
  await toggleScopeNode(dbg, 7);

  is(getLabel(dbg, 7), "a");
  is(getLabel(dbg, 8), "hello");
  is(getLabel(dbg, 9), "<prototype>");
  is(getLabel(dbg, 10), "b");
  is(getLabel(dbg, 11), "hello");
  is(getLabel(dbg, 12), "<prototype>");
  is(getLabel(dbg, 13), "c");
  is(getLabel(dbg, 14), "hello");
  is(getLabel(dbg, 15), "<prototype>");

  info("Expand `e`");
  await toggleScopeNode(dbg, 4);

  info("Expand the `target` node");
  let nodes = getAllLabels(dbg);
  const originalNodesCount = nodes.length;
  const targetNodeIndex = nodes.indexOf("target");
  ok(targetNodeIndex > -1, "Found the target node");
  await toggleScopeNode(dbg, targetNodeIndex);
  nodes = getAllLabels(dbg);
  ok(nodes.length > originalNodesCount, "the target node was expanded");
  ok(nodes.includes("classList"), "classList is displayed");
});

function getAllLabels(dbg) {
  return Array.from(findAllElements(dbg, "scopeNodes")).map(el => el.innerText);
}

function getLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}
