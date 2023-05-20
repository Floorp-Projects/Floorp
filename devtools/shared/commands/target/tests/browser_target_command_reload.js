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

add_task(async function () {
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

add_task(async function () {
  info(" ### Test reloading an Add-on");

  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    background() {
      const { browser } = this;
      browser.test.log("background script executed");
    },
  });

  await extension.startup();

  const commands = await CommandsFactory.forAddon(extension.id);
  const targetCommand = commands.targetCommand;

  // We have to start listening in order to ensure having a targetFront available
  await targetCommand.startListening();

  const { onResource: onReloaded } =
    await commands.resourceCommand.waitForNextResource(
      commands.resourceCommand.TYPES.DOCUMENT_EVENT,
      {
        ignoreExistingResources: true,
        predicate(resource) {
          return resource.name == "dom-loading";
        },
      }
    );

  const backgroundPageURL = targetCommand.targetFront.url;
  ok(backgroundPageURL, "Got the background page URL");
  await targetCommand.reloadTopLevelTarget();

  info("Wait for next dom-loading DOCUMENT_EVENT");
  const event = await onReloaded;

  // If we get about:blank here, it most likely means we receive notification
  // for the previous background page being unload and navigating to about:blank
  is(
    event.url,
    backgroundPageURL,
    "We received the DOCUMENT_EVENT's for the expected document: the new background page."
  );

  await commands.destroy();

  await extension.unload();
});
function getContentVariable() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return content.wrappedJSObject.jsValue;
  });
}
