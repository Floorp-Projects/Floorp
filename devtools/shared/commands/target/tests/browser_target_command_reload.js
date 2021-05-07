/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the TargetCommand's reload method
//
// Note that we reload against main process,
// but this is hard/impossible to test as it reloads the test script itself
// and so stops its execution.

// Load a page with a JS script that change its value everytime we load it
// (that's to see if the reload loads from cache or not)
const TEST_URL = URL_ROOT + "incremental-js-value-script.sjs";

add_task(async function() {
  info(" ### Test reloading a Tab");

  // Create a TargetCommand for a given test tab
  const tab = await addTab(TEST_URL);
  const commands = await CommandsFactory.forTab(tab);
  const targetCommand = commands.targetCommand;

  // We have to start listening in order to ensure having a targetFront available
  await targetCommand.startListening();

  const firstJSValue = await getContentVariable();
  is(firstJSValue, "1", "Got an initial value for the JS variable");

  const onReloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await targetCommand.reloadTopLevelTarget();
  info("Wait for the tab to be reloaded");
  await onReloaded;

  const secondJSValue = await getContentVariable();
  is(
    secondJSValue,
    "1",
    "The first reload didn't bypass the cache, so the JS Script is the same and we got the same value"
  );

  const onSecondReloaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser
  );
  await targetCommand.reloadTopLevelTarget(true);
  info("Wait for the tab to be reloaded");
  await onSecondReloaded;

  // The value is 3 and not 2, because we got a HTTP request, but it returned 304 and the browser fetched his cached content
  const thirdJSValue = await getContentVariable();
  is(
    thirdJSValue,
    "3",
    "The second reload did bypass the cache, so the JS Script is different and we got a new value"
  );

  BrowserTestUtils.removeTab(tab);

  await commands.destroy();
});

add_task(async function() {
  info(" ### Test reloading a content process");

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "data:text/html,foo",
    forceNewProcess: true,
  });

  const { osPid } = tab.linkedBrowser.browsingContext.currentWindowGlobal;

  const commands = await CommandsFactory.forProcess(osPid);

  // We have to start listening in order to ensure having a targetFront available
  await commands.targetCommand.startListening();

  try {
    await commands.targetCommand.reloadTopLevelTarget();
    ok(false, "reloadToLevelTarget() should have thrown for the main process");
  } catch (e) {
    is(
      e.message,
      "The top level target isn't a BrowsingContext and don't support being reloaded"
    );
  }
  await commands.destroy();
});

function getContentVariable() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    return content.wrappedJSObject.jsValue;
  });
}
