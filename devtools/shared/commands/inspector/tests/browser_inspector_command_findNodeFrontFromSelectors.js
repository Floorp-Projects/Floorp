/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  // Build a simple test page with a remote iframe, using two distinct origins .com and .org
  const iframeHtml = encodeURIComponent(`<h2 id="in-iframe"></h2>`);
  const html = encodeURIComponent(
    `<main class="foo bar">
       <button id="child">Click</button>
     </main>
     <iframe src="https://example.org/document-builder.sjs?html=${iframeHtml}"></iframe>`
  );
  const tab = await addTab(
    "https://example.com/document-builder.sjs?html=" + html
  );

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  info("Check that it returns null when no params are passed");
  let nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors();
  is(
    nodeFront,
    null,
    `findNodeFrontFromSelectors returns null when no param is passed`
  );

  info("Check that it returns null when a string is passed");
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors(
    "body main"
  );
  is(
    nodeFront,
    null,
    `findNodeFrontFromSelectors returns null when passed a string`
  );

  info("Check it returns null when an empty array is passed");
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors([]);
  is(
    nodeFront,
    null,
    `findNodeFrontFromSelectors returns null when passed an empty array`
  );

  info("Check that passing a selector for a non-matching element returns null");
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors([
    "h1",
  ]);
  is(
    nodeFront,
    null,
    "findNodeFrontFromSelectors returns null as there's no <h1> element in the page"
  );

  info("Check passing a selector for an element in the top document");
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors([
    "button",
  ]);
  is(
    nodeFront.typeName,
    "domnode",
    "findNodeFrontFromSelectors returns a nodeFront"
  );
  is(
    nodeFront.displayName,
    "button",
    "findNodeFrontFromSelectors returned the appropriate nodeFront"
  );

  info("Check passing a selector for an element in an iframe");
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors([
    "iframe",
    "#in-iframe",
  ]);
  is(
    nodeFront.displayName,
    "h2",
    "findNodeFrontFromSelectors returned the appropriate nodeFront"
  );

  info(
    "Check passing a selector for an non-existing element in an existing iframe"
  );
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors([
    "iframe",
    "#non-existant-id",
  ]);
  is(
    nodeFront.displayName,
    "#document",
    "findNodeFrontFromSelectors returned the last matching iframe document if the children selector isn't found"
  );
  is(
    nodeFront.parentNode().displayName,
    "iframe",
    "findNodeFrontFromSelectors returned the last matching iframe document if the children selector isn't found"
  );

  await commands.destroy();
});
