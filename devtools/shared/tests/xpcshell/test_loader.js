/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  useDistinctSystemPrincipalLoader,
  releaseDistinctSystemPrincipalLoader,
} = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/DistinctSystemPrincipalLoader.sys.mjs"
);

function run_test() {
  const requester = {},
    requester2 = {};

  const loader = useDistinctSystemPrincipalLoader(requester);

  // The DevTools dedicated global forces invisibleToDebugger on its realm at
  // https://searchfox.org/mozilla-central/rev/12a18f7e112a4dcf88d8441d439b84144bfbe9a3/js/xpconnect/loader/mozJSModuleLoader.cpp#591-593
  // but this is not observable directly.
  const DevToolsSpecialGlobal = Cu.getGlobalForObject(
    ChromeUtils.importESModule(
      "resource://devtools/shared/DevToolsInfaillibleUtils.sys.mjs",
      { loadInDevToolsLoader: true }
    )
  );

  const regularLoader = new DevToolsLoader();
  ok(
    DevToolsSpecialGlobal !== regularLoader.loader.sharedGlobal,
    "The regular loader is not using the special DevTools global"
  );

  info("Assert the key difference with the other regular loaders:");
  ok(
    DevToolsSpecialGlobal === loader.loader.sharedGlobal,
    "The system principal loader is using the special DevTools global"
  );

  ok(loader.loader, "Loader is not destroyed before calling release");

  info("Now assert the precise behavior of release");
  releaseDistinctSystemPrincipalLoader({});
  ok(
    loader.loader,
    "Loader is still not destroyed after calling release with another requester"
  );

  releaseDistinctSystemPrincipalLoader(requester);
  ok(
    !loader.loader,
    "Loader is destroyed after calling release with the right requester"
  );

  info("Now test the behavior with two concurrent usages");
  const loader2 = useDistinctSystemPrincipalLoader(requester);
  Assert.notEqual(loader, loader2, "We get a new loader instance");
  ok(
    DevToolsSpecialGlobal === loader2.loader.sharedGlobal,
    "The new system principal loader is also using the special DevTools global"
  );

  const loader3 = useDistinctSystemPrincipalLoader(requester2);
  Assert.equal(loader2, loader3, "The two loader last loaders are shared");

  releaseDistinctSystemPrincipalLoader(requester);
  ok(loader2.loader, "Loader isn't destroy on the first call to destroy");

  releaseDistinctSystemPrincipalLoader(requester2);
  ok(!loader2.loader, "Loader is destroyed on the second call to destroy");
}
