"use strict";

add_task(async function containerIsolation_restricted() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.userContextIsolation.enabled", true],
      ["privacy.userContext.enabled", true],
    ],
  });

  let helperExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["tabs", "cookies", "webNavigation"],
    },

    async background() {
      browser.webNavigation.onCompleted.addListener(details => {
        browser.test.sendMessage("tabCreated", details.tabId);
      });
      browser.test.onMessage.addListener(async message => {
        switch (message.subject) {
          case "createTab": {
            await browser.tabs.create({
              url: message.data.url,
              cookieStoreId: message.data.cookieStoreId,
            });
            break;
          }
        }
      });
    },
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webNavigation"],
    },

    async background() {
      let eventNames = [
        "onBeforeNavigate",
        "onCommitted",
        "onDOMContentLoaded",
        "onCompleted",
      ];

      const initialEmptyTabs = await browser.tabs.query({ active: true });
      browser.test.assertEq(
        1,
        initialEmptyTabs.length,
        `Got one initial empty tab as expected: ${JSON.stringify(
          initialEmptyTabs
        )}`
      );

      for (let eventName of eventNames) {
        browser.webNavigation[eventName].addListener(details => {
          if (details.tabId === initialEmptyTabs[0].id) {
            // Ignore webNavigation related to the initial about:blank tab, it may be technically
            // still being loading when we start this test extension to run the test scenario.
            return;
          }
          browser.test.assertEq(
            "http://www.example.com/?allowed",
            details.url,
            `expected ${eventName} event`
          );
          browser.test.sendMessage(eventName, details.tabId);
        });
      }

      const [
        restrictedTab,
        unrestrictedTab,
        noContainerTab,
      ] = await new Promise(resolve => {
        browser.test.onMessage.addListener(message => resolve(message));
      });

      await browser.test.assertRejects(
        browser.webNavigation.getFrame({
          tabId: restrictedTab,
          frameId: 0,
        }),
        `Invalid tab ID: ${restrictedTab}`,
        "getFrame rejected Promise should pass the expected error"
      );

      await browser.test.assertRejects(
        browser.webNavigation.getAllFrames({ tabId: restrictedTab }),
        `Invalid tab ID: ${restrictedTab}`,
        "getAllFrames rejected Promise should pass the expected error"
      );

      await browser.tabs.remove(unrestrictedTab);
      await browser.tabs.remove(noContainerTab);

      browser.test.sendMessage("done");
    },
  });

  await extension.startup();

  await SpecialPowers.pushPrefEnv({
    set: [
      [`extensions.userContextIsolation.${extension.id}.restricted`, "[1]"],
    ],
  });

  await helperExtension.startup();

  helperExtension.sendMessage({
    subject: "createTab",
    data: {
      url: "http://www.example.com/?restricted",
      cookieStoreId: "firefox-container-1",
    },
  });

  const restrictedTab = await helperExtension.awaitMessage("tabCreated");

  helperExtension.sendMessage({
    subject: "createTab",
    data: {
      url: "http://www.example.com/?allowed",
      cookieStoreId: "firefox-container-2",
    },
  });

  const unrestrictedTab = await helperExtension.awaitMessage("tabCreated");

  helperExtension.sendMessage({
    subject: "createTab",
    data: {
      url: "http://www.example.com/?allowed",
    },
  });

  const noContainerTab = await helperExtension.awaitMessage("tabCreated");

  let eventNames = [
    "onBeforeNavigate",
    "onCommitted",
    "onDOMContentLoaded",
    "onCompleted",
  ];
  for (let eventName of eventNames) {
    let recTabId1 = await extension.awaitMessage(eventName);
    let recTabId2 = await extension.awaitMessage(eventName);

    Assert.equal(
      recTabId1,
      unrestrictedTab,
      `Expected unrestricted tab with tabId: ${unrestrictedTab} from ${eventName} event`
    );

    Assert.equal(
      recTabId2,
      noContainerTab,
      `Expected noContainer tab with tabId: ${noContainerTab} from ${eventName} event`
    );
  }

  extension.sendMessage([restrictedTab, unrestrictedTab, noContainerTab]);

  await extension.awaitMessage("done");

  await extension.unload();
  await helperExtension.unload();
  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
