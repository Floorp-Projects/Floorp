/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const PAGE =
  "http://mochi.test:8888/browser/browser/components/extensions/test/browser/context.html";

add_task(async function accesskeys() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE);
  gBrowser.selectedTab = tab;

  async function background() {
    // description is informative.
    // title is passed to menus.create.
    // label and key are compared with the actual values.
    const TESTCASES = [
      {
        description: "Amp at start",
        title: "&accesskey",
        label: "accesskey",
        key: "a",
      },
      {
        description: "amp in between",
        title: "A& b",
        label: "A b",
        key: " ",
      },
      {
        description: "lonely amp",
        title: "&",
        label: "",
        key: "",
      },
      {
        description: "amp at end",
        title: "End &",
        label: "End ",
        key: "",
      },
      {
        description: "escaped amp",
        title: "A && B",
        label: "A & B",
        key: "",
      },
      {
        description: "amp before escaped amp",
        title: "A &T&& before",
        label: "A T& before",
        key: "T",
      },
      {
        description: "amp after escaped amp",
        title: "A &&&T after",
        label: "A &T after",
        key: "T",
      },
      {
        // Only the first amp should be used as the access key.
        description: "amp, escaped amp, amp to ignore",
        title: "First &1 comes && first &2 serves",
        label: "First 1 comes & first 2 serves",
        key: "1",
      },
      {
        description: "created with amp, updated without amp",
        title: "temp with &X", // will be updated below.
        label: "remove amp",
        key: "",
      },
      {
        description: "created without amp, update with amp",
        title: "temp without access key", // will be updated below.
        label: "add ampY",
        key: "Y",
      },
    ];

    let menuIds = TESTCASES.map(({ title }) => browser.menus.create({ title }));

    // Should clear the access key:
    await browser.menus.update(menuIds[menuIds.length - 2], {
      title: "remove amp",
    });

    // Should add an access key:
    await browser.menus.update(menuIds[menuIds.length - 1], {
      title: "add amp&Y",
    });
    // Should not clear the access key because title is not set:
    await browser.menus.update(menuIds[menuIds.length - 1], { enabled: true });

    browser.test.sendMessage("testCases", TESTCASES);
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background,
  });

  await extension.startup();

  const TESTCASES = await extension.awaitMessage("testCases");
  let menu = await openExtensionContextMenu();
  let items = menu.getElementsByTagName("menuitem");
  is(items.length, TESTCASES.length, "Expected menu items for page");
  TESTCASES.forEach(({ description, label, key }, i) => {
    is(items[i].label, label, `Label for item ${i} (${description})`);
    is(items[i].accessKey, key, `Accesskey for item ${i} (${description})`);
  });

  await closeExtensionContextMenu();
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function accesskeys_selection() {
  const PAGE_WITH_AMPS = "data:text/plain;charset=utf-8,PageSelection&Amp";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    PAGE_WITH_AMPS
  );
  gBrowser.selectedTab = tab;

  async function background() {
    const TESTCASES = [
      {
        description: "Selection without amp",
        title: "percent-s: %s.",
        label: "percent-s: PageSelection&Amp.",
        key: "",
      },
      {
        description: "Selection with amp after %s",
        title: "percent-s: %s &A.",
        label: "percent-s: PageSelection&Amp A.",
        key: "A",
      },
      {
        description: "Selection with amp before %s",
        title: "percent-s: &B %s.",
        label: "percent-s: B PageSelection&Amp.",
        key: "B",
      },
      {
        description: "Amp-percent",
        title: "Amp-percent: &%.",
        label: "Amp-percent: %.",
        key: "%",
      },
      {
        // "&%s" should be treated as "%s", and "ignore this" with amps should be ignored.
        description: "Selection with amp-percent-s",
        title: "Amp-percent-s: &%s.&i&g&n&o&r&e& &t&h&i&s",
        label: "Amp-percent-s: PageSelection&Amp.ignore this",
        // Chrome uses the first character of the selection as access key.
        // Let's not copy that behavior...
        key: "",
      },
      {
        description: "Selection with amp before amp-percent-s",
        title: "Amp-percent-s: &_ &%s.",
        label: "Amp-percent-s: _ PageSelection&Amp.",
        key: "_",
      },
    ];

    let lastMenuId;
    for (let { title } of TESTCASES) {
      lastMenuId = browser.menus.create({ contexts: ["selection"], title });
    }
    // Round-trip to ensure that the menus have been registered.
    await browser.menus.update(lastMenuId, {});
    browser.test.sendMessage("testCases", TESTCASES);
  }
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["menus"],
    },
    background,
  });

  await extension.startup();

  // Select all
  await ContentTask.spawn(gBrowser.selectedBrowser, {}, function*(arg) {
    let doc = content.document;
    let range = doc.createRange();
    let selection = content.getSelection();
    selection.removeAllRanges();
    range.selectNodeContents(doc.body);
    selection.addRange(range);
  });

  const TESTCASES = await extension.awaitMessage("testCases");
  let menu = await openExtensionContextMenu();
  let items = menu.getElementsByTagName("menuitem");
  is(items.length, TESTCASES.length, "Expected menu items for page");
  TESTCASES.forEach(({ description, label, key }, i) => {
    is(items[i].label, label, `Label for item ${i} (${description})`);
    is(items[i].accessKey, key, `Accesskey for item ${i} (${description})`);
  });

  await closeExtensionContextMenu();
  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
