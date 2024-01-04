/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that all debug targets except tabs are sorted alphabetically.
add_task(async function () {
  const thisFirefoxClient = setupThisFirefoxMock();

  thisFirefoxClient.listAddons = () => [
    createAddonData({ id: "addon-b", name: "Addon B" }),
    createAddonData({ id: "addon-c", name: "Addon C" }),
    createAddonData({ id: "addon-a", name: "Addon A" }),
    createAddonData({ id: "tmp-b", name: "Temporary B", temporary: true }),
    createAddonData({ id: "tmp-c", name: "Temporary C", temporary: true }),
    createAddonData({ id: "tmp-a", name: "Temporary A", temporary: true }),
  ];

  thisFirefoxClient.listWorkers = () => {
    return {
      otherWorkers: [
        { id: "worker-b", name: "Worker B" },
        { id: "worker-c", name: "Worker C" },
        { id: "worker-a", name: "Worker A" },
      ],
      sharedWorkers: [
        { id: "shared-worker-b", name: "Shared Worker B" },
        { id: "shared-worker-c", name: "Shared Worker C" },
        { id: "shared-worker-a", name: "Shared Worker A" },
      ],
      serviceWorkers: [
        { id: "service-worker-b", name: "Service Worker B" },
        { id: "service-worker-c", name: "Service Worker C" },
        { id: "service-worker-a", name: "Service Worker A" },
      ],
    };
  };

  thisFirefoxClient.listTabs = () => [
    {
      browserId: 2,
      title: "Tab B",
      url: "https://www.b.com",
      retrieveFavicon: () => {},
    },
    {
      browserId: 3,
      title: "Tab C",
      url: "https://www.c.com",
      retrieveFavicon: () => {},
    },
    {
      browserId: 1,
      title: "Tab A",
      url: "https://www.a.com",
      retrieveFavicon: () => {},
    },
  ];

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  function findTargetIndex(name) {
    const targets = [...document.querySelectorAll(".qa-debug-target-item")];
    return targets.findIndex(target => target.textContent.includes(name));
  }

  function assertTargetOrder(targetNames, message) {
    let isSorted = true;
    for (let i = 1; i < targetNames.length; i++) {
      const index1 = findTargetIndex(targetNames[i - 1]);
      const index2 = findTargetIndex(targetNames[i]);
      if (index1 > index2) {
        isSorted = false;
        info(
          `Targets ${targetNames[i - 1]} and ${targetNames[i]} ` +
            `are not sorted as expected`
        );
      }
    }

    ok(isSorted, message);
  }

  assertTargetOrder(
    ["Tab B", "Tab C", "Tab A"],
    "Tabs are sorted as returned by the back-end"
  );
  assertTargetOrder(
    ["Addon A", "Addon B", "Addon C"],
    "Addons are sorted alphabetically"
  );
  assertTargetOrder(
    ["Temporary A", "Temporary B", "Temporary C"],
    "Temporary addons are sorted alphabetically"
  );
  assertTargetOrder(
    ["Worker A", "Worker B", "Worker C"],
    "Workers are sorted alphabetically"
  );
  assertTargetOrder(
    ["Shared Worker A", "Shared Worker B", "Shared Worker C"],
    "Shared workers are sorted alphabetically"
  );
  assertTargetOrder(
    ["Service Worker A", "Service Worker B", "Service Worker C"],
    "Service workers are sorted alphabetically"
  );

  await removeTab(tab);
});
