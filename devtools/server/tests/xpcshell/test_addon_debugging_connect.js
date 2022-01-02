/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

const { startDebugger } = require("resource://test/webextension-helpers.js");

const { createAppInfo, promiseStartupManager } = AddonTestUtils;

AddonTestUtils.init(this);
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

ExtensionTestUtils.init(this);

function watchFrameUpdates(front) {
  const collected = [];

  const listener = data => {
    collected.push(data);
  };

  front.on("frameUpdate", listener);
  let unsubscribe = () => {
    unsubscribe = null;
    front.off("frameUpdate", listener);
    return collected;
  };

  return unsubscribe;
}

function promiseFrameUpdate(front, matcher = () => true) {
  return new Promise(resolve => {
    const listener = data => {
      if (matcher(data)) {
        resolve();
        front.off("frameUpdate", listener);
      }
    };

    front.on("frameUpdate", listener);
  });
}

// Bug 1302702 - Test connect to a webextension addon
add_task(
  {
    // This test needs to run only when the extension are running in a separate
    // child process, otherwise attachThread would pause the main process and this
    // test would get stuck.
    skip_if: () => !WebExtensionPolicy.useRemoteWebExtensions,
  },
  async function test_webextension_addon_debugging_connect() {
    await promiseStartupManager();

    // Install and start a test webextension.
    const extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      background: function() {
        const { browser } = this;
        browser.test.log("background script executed");
        browser.test.sendMessage("background page ready", window.location.href);
      },
    });
    await extension.startup();
    const bgPageURL = await extension.awaitMessage("background page ready");

    const client = await startDebugger();

    const addonDescriptor = await client.mainRoot.getAddon({
      id: extension.id,
    });
    ok(addonDescriptor, "Got an RDP description");

    // Connect to the target addon actor and wait for the updated list of frames.
    const addonTarget = await addonDescriptor.getTarget();
    ok(addonTarget, "Got an RDP target");

    const { frames } = await addonTarget.listFrames();
    const backgroundPageFrame = frames
      .filter(frame => {
        return (
          frame.url && frame.url.endsWith("/_generated_background_page.html")
        );
      })
      .pop();
    equal(backgroundPageFrame.addonID, extension.id, "Got an extension frame");

    const threadFront = await addonTarget.attachThread();

    ok(threadFront, "Got a threadFront for the target addon");
    equal(threadFront.paused, false, "The addon threadActor isn't paused");

    equal(
      ExtensionParent.DebugUtils.debugBrowserPromises.size,
      1,
      "The expected number of debug browser has been created by the addon actor"
    );

    const unwatchFrameUpdates = watchFrameUpdates(addonTarget);

    const promiseBgPageFrameUpdate = promiseFrameUpdate(addonTarget, data => {
      return data.frames?.some(frame => frame.url === bgPageURL);
    });

    // Reload the addon through the RDP protocol.
    await addonTarget.reload();
    info("Wait background page to be fully reloaded");
    await extension.awaitMessage("background page ready");
    info("Wait background page frameUpdate event");
    await promiseBgPageFrameUpdate;

    equal(
      ExtensionParent.DebugUtils.debugBrowserPromises.size,
      1,
      "The number of debug browser has not been changed after an addon reload"
    );

    const frameUpdates = unwatchFrameUpdates();
    const [frameUpdate] = frameUpdates;

    equal(
      frameUpdates.length,
      1,
      "Expect 1 frameUpdate events to have been received"
    );
    equal(
      frameUpdate.frames?.length,
      1,
      "Expect 1 frame in the frameUpdate event "
    );
    Assert.deepEqual(
      {
        url: frameUpdate.frames[0].url,
        addonID: frameUpdate.frames[0].addonID,
      },
      {
        url: bgPageURL,
        addonID: extension.id,
      },
      "Got the expected frame update when the addon background page was loaded back"
    );

    await client.close();

    // Check that if we close the debugging client without uninstalling the addon,
    // the webextension debugging actor should release the debug browser.
    equal(
      ExtensionParent.DebugUtils.debugBrowserPromises.size,
      0,
      "The debug browser has been released when the RDP connection has been closed"
    );

    await extension.unload();
  }
);
