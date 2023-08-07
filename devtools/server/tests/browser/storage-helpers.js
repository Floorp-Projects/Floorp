/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This file assumes head.js is loaded in the global scope.
/* import-globals-from head.js */

/* exported openTabAndSetupStorage, clearStorage */

"use strict";

/**
 * This generator function opens the given url in a new tab, then sets up the
 * page by waiting for all cookies, indexedDB items etc. to be created.
 *
 * @param url {String} The url to be opened in the new tab
 *
 * @return {Promise} A promise that resolves after storage inspector is ready
 */
async function openTabAndSetupStorage(url) {
  await addTab(url);

  // Setup the async storages in main window and for all its iframes
  const browsingContexts =
    gBrowser.selectedBrowser.browsingContext.getAllBrowsingContextsInSubtree();
  for (const browsingContext of browsingContexts) {
    await SpecialPowers.spawn(browsingContext, [], async function () {
      if (content.wrappedJSObject.setup) {
        await content.wrappedJSObject.setup();
      }
    });
  }

  // selected tab is set in addTab
  const commands = await CommandsFactory.forTab(gBrowser.selectedTab);
  await commands.targetCommand.startListening();
  const target = commands.targetCommand.targetFront;
  return { commands, target };
}

async function clearStorage() {
  const browsingContexts =
    gBrowser.selectedBrowser.browsingContext.getAllBrowsingContextsInSubtree();
  for (const browsingContext of browsingContexts) {
    await SpecialPowers.spawn(browsingContext, [], async function () {
      if (content.wrappedJSObject.clear) {
        await content.wrappedJSObject.clear();
      }
    });
  }
}
