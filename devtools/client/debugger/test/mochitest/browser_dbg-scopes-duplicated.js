/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
      var sharedValue = {hello: "world"};
      var x = {
        a: sharedValue,
        b: sharedValue,
        c: sharedValue,
        d: {
          e: {
            g: sharedValue
          },
          f: {
            h: sharedValue
          }
        }
      };
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

  checkScopesLabels(
    dbg,
    `
    | e
    | sharedValue
    | x
    `,
    { startIndex: 3 }
  );

  info("Expand `x` node");
  await toggleScopeNode(dbg, 6);
  checkScopesLabels(
    dbg,
    `
    | x
    | | a
    | | b
    | | c
    | | d
    `,
    { startIndex: 5 }
  );

  info("Expand node `d`");
  await toggleScopeNode(dbg, 10);
  checkScopesLabels(
    dbg,
    `
    | | d
    | | | e
    | | | f
    `,
    { startIndex: 9 }
  );

  info("Expand `f` and `e` nodes");
  await toggleScopeNode(dbg, 12);
  await toggleScopeNode(dbg, 11);
  checkScopesLabels(
    dbg,
    `
    | | d
    | | | e
    | | | | g
    | | | | <prototype>
    | | | f
    | | | | h
    | | | | <prototype>
    `,
    { startIndex: 9 }
  );

  info("Expand `h`, `g`, `e`, `c`, `b` and `a` nodes");
  await toggleScopeNode(dbg, 15);
  await toggleScopeNode(dbg, 12);
  await toggleScopeNode(dbg, 9);
  await toggleScopeNode(dbg, 8);
  await toggleScopeNode(dbg, 7);

  checkScopesLabels(
    dbg,
    `
    | x
    | | a
    | | | hello
    | | | <prototype>
    | | b
    | | | hello
    | | | <prototype>
    | | c
    | | | hello
    | | | <prototype>
    | | d
    | | | e
    | | | | g
    | | | | | hello
    | | | | | <prototype>
    | | | | <prototype>
    | | | f
    | | | | h
    | | | | | hello
    | | | | | <prototype>
    | | | | <prototype>
    `,
    { startIndex: 5 }
  );

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

function getAllLabels(dbg, withIndent = false) {
  return Array.from(findAllElements(dbg, "scopeNodes")).map(el => {
    let text = el.innerText;
    if (withIndent) {
      const node = el.closest(".tree-node");
      const level = Number(node.getAttribute("aria-level"));
      if (!Number.isNaN(level)) {
        text = `${"| ".repeat(level - 1)}${text}`.trim();
      }
    }
    return text;
  });
}

function checkScopesLabels(dbg, expected, { startIndex = 0 } = {}) {
  const lines = expected
    .trim()
    .split("\n")
    .map(line => line.trim());

  const labels = getAllLabels(dbg, true).slice(
    startIndex,
    startIndex + lines.length
  );

  const format = arr => `\n${arr.join("\n")}\n`;
  is(format(labels), format(lines), "got expected scope labels");
}

function getLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}
