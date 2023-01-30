// Import this in order to use `do_tests()`.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/ipc/tests/browser_wpi_base.js",
  this
);

add_task(async function test_isolate_nothing() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatedMozillaDomains", "mozilla.org"],
      [
        "fission.webContentIsolationStrategy",
        WebContentIsolationStrategy.IsolateNothing,
      ],
    ],
  });

  await do_tests({
    com_normal: "web",
    org_normal: "web",
    moz_normal: "privilegedmozilla",
    com_high: "web",
    com_coop_coep: "webCOOP+COEP=https://example.com",
    org_coop_coep: "webCOOP+COEP=https://example.org",
    moz_coop_coep: "privilegedmozilla",
  });
});
