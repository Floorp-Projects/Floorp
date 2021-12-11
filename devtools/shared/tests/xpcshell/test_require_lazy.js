/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { loader } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
// Test devtools.lazyRequireGetter

function run_test() {
  const name = "asyncUtils";
  const path = "devtools/shared/async-utils";
  const o = {};
  loader.lazyRequireGetter(o, name, path);
  const asyncUtils = require(path);
  // XXX: do_check_eq only works on primitive types, so we have this
  // do_check_true of an equality expression.
  Assert.ok(o.asyncUtils === asyncUtils);

  // A non-main loader should get a new object via |lazyRequireGetter|, just
  // as it would via a direct |require|.
  const o2 = {};
  const loader2 = new DevToolsLoader();

  // We have to init the loader by loading any module before
  // lazyRequireGetter is available
  loader2.require("devtools/shared/DevToolsUtils");

  loader2.lazyRequireGetter(o2, name, path);
  Assert.ok(o2.asyncUtils !== asyncUtils);

  // A module required via a non-main loader that then uses |lazyRequireGetter|
  // should also get the same object from that non-main loader.
  const exposeLoader = loader2.require("xpcshell-test/exposeLoader");
  const o3 = exposeLoader.exerciseLazyRequire(name, path);
  Assert.ok(o3.asyncUtils === o2.asyncUtils);
}
