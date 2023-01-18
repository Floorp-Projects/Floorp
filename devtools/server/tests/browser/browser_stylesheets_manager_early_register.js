/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CSS_CONTENT = "body { background-color: red; }";
const TEST_URI = `data:text/html;charset=utf-8,<style>${encodeURIComponent(
  CSS_CONTENT
)}</style>`;

// Test that registering stylesheets early in the styleSheetsManager does not
// prevent from receiving those stylesheets when a consumer starts watching for
// this resource.
add_task(async function() {
  await addTab(TEST_URI);

  // addTab is overridden in this suite and returns a `browser` element, so we
  // can't use the return value here.
  const tab = gBrowser.selectedTab;

  const commands = await CommandsFactory.forTab(tab);
  const { resourceCommand, targetCommand } = commands;
  await targetCommand.startListening();

  const connectionPrefix = targetCommand.watcherFront.actorID.replace(
    /watcher\d+$/,
    ""
  );

  info("Force the stylesheet manager to register the page's stylesheet");
  const resourceId = await ContentTask.spawn(
    tab.linkedBrowser,
    [connectionPrefix],
    function(_connectionPrefix) {
      const { TargetActorRegistry } = ChromeUtils.import(
        "resource://devtools/server/actors/targets/target-actor-registry.jsm"
      );

      // Retrieve the target actor instance and its watcher for console messages
      const targetActor = TargetActorRegistry.getTopLevelTargetActorForContext(
        {
          type: "browser-element",
          browserId: content.browsingContext.browserId,
        },
        _connectionPrefix
      );

      // Register the page's stylesheet and return the resource id.
      const styleSheetsManager = targetActor.getStyleSheetsManager();
      const styleSheet =
        content.browsingContext.associatedWindow.document.styleSheets[0];
      return styleSheetsManager.getStyleSheetResourceId(styleSheet);
    }
  );

  is(typeof resourceId, "string", "Retrieved a valid resource id");

  info("Start watching for stylesheet resources");
  const stylesheetResources = [];
  await resourceCommand.watchResources([resourceCommand.TYPES.STYLESHEET], {
    onAvailable: resources => stylesheetResources.push(...resources),
  });

  info("Wait until we retrieve the expected resource");
  const styleSheetResource = await waitFor(() =>
    stylesheetResources.find(resource => resource.resourceId === resourceId)
  );
  ok(
    styleSheetResource,
    "Found the stylesheet resource registered in the stylesheet manager earlier"
  );

  await commands.destroy();
});
