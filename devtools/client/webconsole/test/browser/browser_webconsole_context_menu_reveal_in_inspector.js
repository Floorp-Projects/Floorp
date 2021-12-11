/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the "Reveal in Inspector" menu item of the webconsole is enabled only when
// clicking on HTML elements attached to the DOM. Also check that clicking the menu
// item or using the access-key Q does select the node in the inspector.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,
  <!DOCTYPE html>
  <html>
    <body></body>
    <script>
      console.log("foo");
      console.log({hello: "world"});
      console.log(document.createElement("span"));
      console.log(document.body.appendChild(document.createElement("div")));
      console.log(document.body.appendChild(document.createTextNode("test-text")));
      console.log(document.querySelectorAll('html'));
    </script>
  </html>
`;
const revealInInspectorMenuItemId = "#console-menu-open-node";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const msgWithText = await waitFor(() => findMessage(hud, `foo`));
  const msgWithObj = await waitFor(() => findMessage(hud, `Object`));
  const nonDomEl = await waitFor(() =>
    findMessage(hud, `<span>`, ".objectBox-node")
  );

  const domEl = await waitFor(() =>
    findMessage(hud, `<div>`, ".objectBox-node")
  );
  const domTextEl = await waitFor(() =>
    findMessage(hud, `test-text`, ".objectBox-textNode")
  );
  const domElCollection = await waitFor(() =>
    findMessage(hud, `html`, ".objectBox-node")
  );

  info("Check `Reveal in Inspector` is not visible for strings");
  await testRevealInInspectorDisabled(hud, msgWithText);

  info("Check `Reveal in Inspector` is not visible for objects");
  await testRevealInInspectorDisabled(hud, msgWithObj);

  info("Check `Reveal in Inspector` is not visible for disconnected nodes");
  await testRevealInInspectorDisabled(hud, nonDomEl);

  info("Check `Reveal in Inspector` for a single connected node");
  await testRevealInInspector(hud, domEl, "div", false);

  info("Check `Reveal in Inspector` for a connected text element");
  await testRevealInInspector(hud, domTextEl, "#text", false);

  info("Check `Reveal in Inspector` for a collection of elements");
  await testRevealInInspector(hud, domElCollection, "html", false);

  info("`Reveal in Inspector` by using the access-key Q");
  await testRevealInInspector(hud, domEl, "div", true);
});

async function testRevealInInspector(hud, element, tag, accesskey) {
  if (
    !accesskey &&
    AppConstants.platform == "macosx" &&
    Services.prefs.getBoolPref("widget.macos.native-context-menus", false)
  ) {
    info(
      "Not testing accesskey behaviour since we can't use synthesized keypresses in macOS native menus."
    );
    return;
  }
  const toolbox = hud.toolbox;

  // Loading the inspector panel at first, to make it possible to listen for
  // new node selections
  await toolbox.loadTool("inspector");
  const inspector = toolbox.getPanel("inspector");

  const menuPopup = await openContextMenu(hud, element);
  const revealInInspectorMenuItem = menuPopup.querySelector(
    revealInInspectorMenuItemId
  );
  ok(
    revealInInspectorMenuItem !== null,
    "There is the `Reveal in Inspector` menu item"
  );

  const onInspectorSelected = toolbox.once("inspector-selected");
  const onInspectorUpdated = inspector.once("inspector-updated");
  const onNewNode = toolbox.selection.once("new-node-front");

  if (accesskey) {
    info("Clicking on `Reveal in Inspector` menu item");
    menuPopup.activateItem(revealInInspectorMenuItem);
  } else {
    info("Using access-key Q to `Reveal in Inspector`");
    await synthesizeKeyShortcut("Q");
  }

  await onInspectorSelected;
  await onInspectorUpdated;
  const nodeFront = await onNewNode;

  ok(true, "Inspector selected and new node got selected");
  is(nodeFront.displayName, tag, "The expected node was selected");

  await openConsole();
}

async function testRevealInInspectorDisabled(hud, element) {
  info("Check 'Reveal in Inspector' is not in the menu");
  const menuPopup = await openContextMenu(hud, element);
  const revealInInspectorMenuItem = menuPopup.querySelector(
    revealInInspectorMenuItemId
  );
  ok(
    !revealInInspectorMenuItem,
    `"Reveal in Inspector" is not available for messages with no HTML DOM elements`
  );
  await hideContextMenu(hud);
}
