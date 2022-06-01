/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  useDistinctSystemPrincipalLoader,
  releaseDistinctSystemPrincipalLoader,
} = ChromeUtils.import("resource://devtools/shared/loader/Loader.jsm");

function run_test() {
  const requester = {},
    requester2 = {};
  const loader = useDistinctSystemPrincipalLoader(requester);

  info("Assert the key difference with the other regular loaders:");
  ok(
    loader.loader.invisibleToDebugger,
    "The loader is set invisible to debugger"
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
    loader2.loader.invisibleToDebugger,
    "The loader is set invisible to debugger"
  );

  const loader3 = useDistinctSystemPrincipalLoader(requester2);
  Assert.equal(loader2, loader3, "The two loader last loaders are shared");

  releaseDistinctSystemPrincipalLoader(requester);
  ok(loader2.loader, "Loader isn't destroy on the first call to destroy");

  releaseDistinctSystemPrincipalLoader(requester2);
  ok(!loader2.loader, "Loader is destroyed on the second call to destroy");
}
