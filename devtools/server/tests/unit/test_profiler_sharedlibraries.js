/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests whether the profiler responds to "sharedLibraries" adequately.
 */

function run_test() {
  get_chrome_actors((client, form) => {
    let actor = form.profilerActor;
    test_sharedlibraries(client, actor, () => {
      client.close().then(() => {
        do_test_finished();
      });
    });
  });

  do_test_pending();
}

function test_sharedlibraries(client, actor, callback) {
  client.request({ to: actor, type: "sharedLibraries" }, response => {
    const libs = response.sharedLibraries;
    do_check_eq(typeof libs, "object");
    do_check_true(Array.isArray(libs));
    do_check_eq(typeof libs, "object");
    do_check_true(libs.length >= 1);
    do_check_eq(typeof libs[0], "object");
    do_check_eq(typeof libs[0].name, "string");
    do_check_eq(typeof libs[0].path, "string");
    do_check_eq(typeof libs[0].debugName, "string");
    do_check_eq(typeof libs[0].debugPath, "string");
    do_check_eq(typeof libs[0].arch, "string");
    do_check_eq(typeof libs[0].start, "number");
    do_check_eq(typeof libs[0].end, "number");
    do_check_true(libs[0].start <= libs[0].end);
    callback();
  });
}
