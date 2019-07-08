add_task(async function test_privileged_remote_true() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedContentProcess", true],
      ["browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true],
      ["browser.tabs.remote.separatedMozillaDomains", "example.org"],
    ],
  });

  test_url_for_process_types(
    "https://example.com",
    false,
    true,
    false,
    false,
    false
  );
  test_url_for_process_types(
    "https://example.org",
    false,
    false,
    false,
    true,
    false
  );
});

add_task(async function test_privileged_remote_false() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedContentProcess", true],
      ["browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", false],
    ],
  });

  test_url_for_process_types(
    "https://example.com",
    false,
    true,
    false,
    false,
    false
  );
  test_url_for_process_types(
    "https://example.org",
    false,
    true,
    false,
    false,
    false
  );
});
