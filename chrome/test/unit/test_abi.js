const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);
updateAppInfo({
  name: "XPCShell",
  ID: "{39885e5f-f6b4-4e2a-87e5-6259ecf79011}",
  version: "5",
  platformVersion: "1.9",
});

registerManifests([do_get_file("data/test_abi.manifest")]);

function is_registered(name) {
  try {
    var d = Services.catMan.getCategoryEntry("abitest", name);
    return d == "found";
  } catch (e) {
    return false;
  }
}

function run_test() {
  Assert.ok(is_registered("test1"));
  Assert.ok(!is_registered("test2"));
  Assert.ok(is_registered("test3"));
  Assert.ok(!is_registered("test4"));
}
