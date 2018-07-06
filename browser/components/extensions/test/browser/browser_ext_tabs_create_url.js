/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_urlbar_focus() {
  const extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.tabs.onUpdated.addListener(function onUpdated(_, info) {
        if (info.status === "complete") {
          browser.test.sendMessage("complete");
          browser.tabs.onUpdated.removeListener(onUpdated);
        }
      });
      browser.test.onMessage.addListener(async (cmd, ...args) => {
        const result = await browser.tabs[cmd](...args);
        browser.test.sendMessage("result", result);
      });
    },
  });

  await extension.startup();

  // Test content is focused after opening a regular url
  extension.sendMessage("create", {url: "https://example.com"});
  const [tab1] = await Promise.all([
    extension.awaitMessage("result"),
    extension.awaitMessage("complete"),
  ]);

  is(document.activeElement.tagName, "browser", "Content focused after opening a web page");

  extension.sendMessage("remove", tab1.id);
  await extension.awaitMessage("result");

  // Test urlbar is focused after opening an empty tab
  extension.sendMessage("create", {});
  const tab2 = await extension.awaitMessage("result");

  const active = document.activeElement;
  info(`Active element: ${active.tagName}, id: ${active.id}, class: ${active.className}`);

  const parent = active.parentNode;
  info(`Parent element: ${parent.tagName}, id: ${parent.id}, class: ${parent.className}`);

  info(`After opening an empty tab, gURLBar.focused: ${gURLBar.focused}`);


  is(active.tagName, "html:input", "Input element focused");
  ok(active.classList.contains("urlbar-input"), "Urlbar focused");

  extension.sendMessage("remove", tab2.id);
  await extension.awaitMessage("result");

  await extension.unload();
});
