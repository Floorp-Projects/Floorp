/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
var {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const defer = require("devtools/shared/promise_defer");

function testResolve() {
  const deferred = defer();
  deferred.resolve("success");
  return deferred.promise;
}

function testReject() {
  const deferred = defer();
  deferred.reject("error");
  return deferred.promise;
}

add_task(function* () {
  const success = yield testResolve();
  equal(success, "success");

  let error;
  try {
    yield testReject();
  } catch (e) {
    error = e;
  }

  equal(error, "error");
});
