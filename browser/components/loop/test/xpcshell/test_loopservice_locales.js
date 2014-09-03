/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test_locale() {
  // Set the pref to something controlled.
  Services.prefs.setCharPref("general.useragent.locale", "ab-CD");

  Assert.equal(MozLoopService.locale, "ab-CD");

  Services.prefs.clearUserPref("general.useragent.locale");
}

function test_getStrings() {
  // Try an invalid string
  Assert.equal(MozLoopService.getStrings("invalid_not_found_string"), "");

  // XXX This depends on the L10n values, which I'd prefer not to do, but is the
  // simplest way for now.
  Assert.equal(MozLoopService.getStrings("share_link_header_text"),
               '{"textContent":"Share this link to invite someone to talk:"}');
}

function run_test()
{
  setupFakeLoopServer();

  test_locale();
  test_getStrings();
}
