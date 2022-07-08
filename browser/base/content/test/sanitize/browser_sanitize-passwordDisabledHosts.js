// Bug 474792 - Clear "Never remember passwords for this site" when
// clearing site-specific settings in Clear Recent History dialog

add_task(async function() {
  // getLoginSavingEnabled always returns false if password capture is disabled.
  await SpecialPowers.pushPrefEnv({ set: [["signon.rememberSignons", true]] });

  // Add a disabled host
  Services.logins.setLoginSavingEnabled("https://example.com", false);
  // Sanity check
  is(
    Services.logins.getLoginSavingEnabled("https://example.com"),
    false,
    "example.com should be disabled for password saving since we haven't cleared that yet."
  );

  // Clear it
  await Sanitizer.sanitize(["siteSettings"], { ignoreTimespan: false });

  // Make sure it's gone
  is(
    Services.logins.getLoginSavingEnabled("https://example.com"),
    true,
    "example.com should be enabled for password saving again now that we've cleared."
  );

  await SpecialPowers.popPrefEnv();
});
