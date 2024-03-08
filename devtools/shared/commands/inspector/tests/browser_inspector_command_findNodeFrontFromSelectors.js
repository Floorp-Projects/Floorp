/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  // Build a simple test page with a remote iframe, using two distinct origins .com and .org
  const iframeOrgHtml = encodeURIComponent(
    `<h2 id="in-iframe">in org - same origin</h2>`
  );
  const iframeComHtml = encodeURIComponent(`<h3>in com - remote</h3>`);
  const html = encodeURIComponent(
    `<main class="foo bar">
       <button id="child">Click</button>
     </main>
     <!-- adding delay to both iframe so we can check we handle loading document has expected -->
     <iframe id="iframe-org" src="https://example.org/document-builder.sjs?delay=3000&html=${iframeOrgHtml}"></iframe>
     <iframe id="iframe-com" src="https://example.com/document-builder.sjs?delay=6000&html=${iframeComHtml}"></iframe>`
  );
  const tab = await addTab(
    "https://example.org/document-builder.sjs?html=" + html,
    { waitForLoad: false }
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

  info("Check passing a selector for an element in a same origin iframe");
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors([
    "#iframe-org",
    "#in-iframe",
  ]);
  is(
    nodeFront.displayName,
    "h2",
    "findNodeFrontFromSelectors returned the appropriate nodeFront"
  );

  info("Check passing a selector for an element in a cross origin iframe");
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors([
    "#iframe-com",
    "h3",
  ]);
  is(
    nodeFront.displayName,
    "h3",
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

  info("Check that timeout does work");
  // Reload the page so we'll have the iframe loading (for 3s) and we can check that
  // putting a smaller timeout will result in the function returning null.
  // we need to wait until it's fully processed to avoid pending promises.
  const onNewTargetProcessed = commands.targetCommand.once(
    "processed-available-target"
  );
  await reloadBrowser({ waitForLoad: false });
  await onNewTargetProcessed;
  nodeFront = await commands.inspectorCommand.findNodeFrontFromSelectors(
    ["#iframe-org", "#in-iframe"],
    // timeout in ms (smaller than 3s)
    100
  );
  is(
    nodeFront,
    null,
    "findNodeFrontFromSelectors timed out and returned null, as expected"
  );

  await commands.destroy();
});
