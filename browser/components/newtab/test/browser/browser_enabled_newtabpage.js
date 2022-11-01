function getSpec(uri) {
  const { spec } = NetUtil.newChannel({
    loadUsingSystemPrincipal: true,
    uri,
  }).URI;

  info(`got ${spec} for ${uri}`);
  return spec;
}

add_task(async function test_newtab_enabled() {
  ok(
    !getSpec("about:newtab").endsWith("/blanktab.html"),
    "did not get blank for default about:newtab"
  );
  ok(
    !getSpec("about:home").endsWith("/blanktab.html"),
    "did not get blank for default about:home"
  );

  await SpecialPowers.pushPrefEnv({
    set: [["browser.newtabpage.enabled", false]],
  });

  ok(
    getSpec("about:newtab").endsWith("/blanktab.html"),
    "got special blank page when newtab is not enabled"
  );
  ok(
    !getSpec("about:home").endsWith("/blanktab.html"),
    "got special blank page for about:home"
  );
});
