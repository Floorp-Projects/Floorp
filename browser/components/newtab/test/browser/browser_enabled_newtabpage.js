ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

function checkSpec(uri, check, message) {
  const {spec} = NetUtil.newChannel({
    loadUsingSystemPrincipal: true,
    uri,
  }).URI;

  info(`got ${spec} for ${uri}`);
  check(spec, "about:blank", message);
}

add_task(async function test_newtab_enabled() {
  checkSpec("about:newtab", isnot, "did not get blank for default about:newtab");
  checkSpec("about:home", isnot, "did not get blank for default about:home");

  await SpecialPowers.pushPrefEnv({set: [["browser.newtabpage.enabled", false]]});

  checkSpec("about:newtab", is, "got blank when newtab is not enabled");
  checkSpec("about:home", isnot, "still did not get blank for about:home");
});
