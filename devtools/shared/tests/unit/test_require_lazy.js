/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { devtools } = Cu.import("resource://devtools/shared/Loader.jsm", {});
// Test devtools.lazyRequireGetter

function run_test() {
  const name = "asyncUtils";
  const path = "devtools/shared/async-utils";
  const o = {};
  devtools.lazyRequireGetter(o, name, path);
  const asyncUtils = require(path);
  // XXX: do_check_eq only works on primitive types, so we have this
  // do_check_true of an equality expression.
  do_check_true(o.asyncUtils === asyncUtils);

  // A non-main loader should get a new object via |lazyRequireGetter|, just
  // as it would via a direct |require|.
  const o2 = {};
  let loader = new DevToolsLoader();

  // We have to init the loader by loading any module before
  // lazyRequireGetter is available
  loader.require("devtools/shared/DevToolsUtils");

  loader.lazyRequireGetter(o2, name, path);
  do_check_true(o2.asyncUtils !== asyncUtils);

  // A module required via a non-main loader that then uses |lazyRequireGetter|
  // should also get the same object from that non-main loader.
  let exposeLoader = loader.require("xpcshell-test/exposeLoader");
  const o3 = exposeLoader.exerciseLazyRequire(name, path);
  do_check_true(o3.asyncUtils === o2.asyncUtils);
}
