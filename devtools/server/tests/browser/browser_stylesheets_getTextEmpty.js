/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that StyleSheetsActor.getText handles empty text correctly.

const CSS_CONTENT = "body { background-color: #f06; }";
const TEST_URI = `data:text/html;charset=utf-8,<style>${encodeURIComponent(
  CSS_CONTENT
)}</style>`;

add_task(async function() {
  const target = await addTabTarget(TEST_URI);

  const {
    ResourceWatcher,
  } = require("devtools/shared/resources/resource-watcher");

  const commands = await target.descriptorFront.getCommands();
  const targetList = commands.targetCommand;
  await targetList.startListening();
  const resourceWatcher = new ResourceWatcher(targetList);

  const styleSheetsFront = await target.getFront("stylesheets");
  ok(styleSheetsFront, "The StyleSheetsFront was created.");

  const sheets = [];
  await resourceWatcher.watchResources([ResourceWatcher.TYPES.STYLESHEET], {
    onAvailable: resources => sheets.push(...resources),
  });
  is(sheets.length, 1, "watchResources returned the correct number of sheets");

  const { resourceId } = sheets[0];

  is(
    await getStyleSheetText(styleSheetsFront, resourceId),
    CSS_CONTENT,
    "The stylesheet has expected initial text"
  );
  info("Update stylesheet content via the styleSheetsFront");
  await styleSheetsFront.update(resourceId, "", false);
  is(
    await getStyleSheetText(styleSheetsFront, resourceId),
    "",
    "Stylesheet is now empty, as expected"
  );

  await target.destroy();
});

async function getStyleSheetText(styleSheetsFront, resourceId) {
  const longStringFront = await styleSheetsFront.getText(resourceId);
  return longStringFront.string();
}
