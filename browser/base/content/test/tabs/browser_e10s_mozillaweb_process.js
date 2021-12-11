add_task(async function test_privileged_remote_true() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedContentProcess", true],
      ["browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true],
      ["browser.tabs.remote.separatedMozillaDomains", "example.org"],
    ],
  });

  test_url_for_process_types({
    url: "https://example.com",
    chromeResult: false,
    webContentResult: true,
    privilegedAboutContentResult: false,
    privilegedMozillaContentResult: false,
    extensionProcessResult: false,
  });
  test_url_for_process_types({
    url: "https://example.org",
    chromeResult: false,
    webContentResult: false,
    privilegedAboutContentResult: false,
    privilegedMozillaContentResult: true,
    extensionProcessResult: false,
  });
});

add_task(async function test_privileged_remote_false() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedContentProcess", true],
      ["browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", false],
    ],
  });

  test_url_for_process_types({
    url: "https://example.com",
    chromeResult: false,
    webContentResult: true,
    privilegedAboutContentResult: false,
    privilegedMozillaContentResult: false,
    extensionProcessResult: false,
  });
  test_url_for_process_types({
    url: "https://example.org",
    chromeResult: false,
    webContentResult: true,
    privilegedAboutContentResult: false,
    privilegedMozillaContentResult: false,
    extensionProcessResult: false,
  });
});
