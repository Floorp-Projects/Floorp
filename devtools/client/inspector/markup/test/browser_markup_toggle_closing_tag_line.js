/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a closing tag line is displayed when expanding an element container.
// Also check that no closing tag line is displayed for readonly containers (document,
// roots...).

const TEST_URL = `data:text/html;charset=utf8,
<div class="outer-div"><span>test</span></div>
<iframe src="data:text/html;charset=utf8,<div>test</div>"></iframe>`;

add_task(async function() {
  const {inspector} = await openInspectorForURL(TEST_URL);

  info("Getting the container for .outer-div parent element");
  let container = await getContainerForSelector(".outer-div", inspector);
  await expandContainerByClick(inspector, container);

  let closeTagLine = container.closeTagLine;
  ok(closeTagLine && closeTagLine.textContent.includes("div"),
     "DIV has a close tag-line with the correct content");

  info("Expand the iframe element");
  container = await getContainerForSelector("iframe", inspector);
  await expandContainerByClick(inspector, container);
  ok(container.expanded, "iframe is expanded");
  closeTagLine = container.closeTagLine;
  ok(closeTagLine && closeTagLine.textContent.includes("iframe"),
     "IFRAME has a close tag-line with the correct content");

  info("Retrieve the nodefront for the #document root inside the iframe");
  const iframe = await getNodeFront("iframe", inspector);
  const {nodes} = await inspector.walker.children(iframe);
  const documentFront = nodes[0];
  ok(documentFront.displayName === "#document", "First child of IFRAME is #document");

  info("Expand the iframe's #document node element");
  container = getContainerForNodeFront(documentFront, inspector);
  await expandContainerByClick(inspector, container);
  ok(container.expanded, "#document is expanded");
  ok(!container.closeTagLine, "readonly (#document) node has no close tag-line");
});
