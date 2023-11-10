"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

add_task(async function helper() {
  do_get_profile();

  // The SearchService is also needed in order to construct the initial state,
  // which means that the AddonManager needs to be available.
  await AddonTestUtils.promiseStartupManager();

  // The example.com domain will be used to host the dynamic layout JSON and
  // the top stories JSON.
  let server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
  server.registerDirectory("/", do_get_cwd());

  Assert.equal(true, fetch instanceof Function);
  var k = await fetch("http://example.com/");
  console.log(k);
  console.log(k.body);
  var r = k.body.getReader();
  console.log(r);
  var v = await r.read();
  console.log(v);
});
