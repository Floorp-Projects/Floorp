/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// 1px red dot
const shortDataUrl =
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIW2P4z8DwHwAFAAH/F1FwBgAAAABJRU5ErkJggg==";

// Not a valid base64 image, but will still generate a long text property
const longDataUrl = new Array(1000 * 1000).join("a");

const TEST_STYLESHEET = `
body {
  background-image: url(data:image/png;base64,${shortDataUrl});
  background-image: url(data:image/png;base64,${longDataUrl});
}`;

// Serve the stylesheet dynamically from a test HTTPServer to avoid logging an
// extremely long data-url when adding the tab using our usual test helpers.
const server = createTestHTTPServer();
const filepath = "/style.css";
const cssuri = `http://localhost:${server.identity.primaryPort}${filepath}`;
server.registerContentType("css", "text/css");
server.registerPathHandler(filepath, (metadata, response) => {
  response.write(TEST_STYLESHEET);
});

const TEST_URL =
  "data:text/html," +
  encodeURIComponent(`
  <!DOCTYPE html>
  <html>
    <head>
      <link href="${cssuri}" rel="stylesheet" />
    </head>
    <body></body>
  </html>
`);

// Check that long URLs are rendered correctly in the rule view.
add_task(async function () {
  await pushPref("devtools.inspector.showRulesViewEnterKeyNotice", false);
  const { inspector } = await openInspectorForURL(TEST_URL);
  const view = selectRuleView(inspector);

  await selectNode("body", inspector);

  const propertyValues = view.styleDocument.querySelectorAll(
    ".ruleview-propertyvalue"
  );

  is(propertyValues.length, 2, "Ruleview has 2 propertyvalue elements");
  ok(
    propertyValues[0].textContent.startsWith("url(data:image/png"),
    "Property value for the background image was correctly rendered"
  );

  ok(
    !propertyValues[0].querySelector(".propertyvalue-long-text"),
    "The first background-image is short enough and does not need to be truncated"
  );
  ok(
    !!propertyValues[1].querySelector(".propertyvalue-long-text"),
    "The second background-image has a special CSS class to be truncated"
  );
  const ruleviewContainer =
    view.styleDocument.getElementById("ruleview-container");
  ok(
    ruleviewContainer.scrollHeight === ruleviewContainer.clientHeight,
    "The ruleview container does not have a scrollbar"
  );
});
