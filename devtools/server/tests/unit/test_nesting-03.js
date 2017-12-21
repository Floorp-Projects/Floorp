/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

// Test that we can detect nested event loops in tabs with the same URL.

var gClient1, gClient2, gThreadClient1, gThreadClient2;

function run_test() {
  initTestDebuggerServer();
  addTestGlobal("test-nesting1");
  addTestGlobal("test-nesting1");
  // Conect the first client to the first debuggee.
  gClient1 = new DebuggerClient(DebuggerServer.connectPipe());
  gClient1.connect(function () {
    attachTestThread(gClient1, "test-nesting1",
                     function (response, tabClient, threadClient) {
                       gThreadClient1 = threadClient;
                       start_second_connection();
                     });
  });
  do_test_pending();
}

function start_second_connection() {
  gClient2 = new DebuggerClient(DebuggerServer.connectPipe());
  gClient2.connect(function () {
    attachTestThread(gClient2, "test-nesting1",
                     function (response, tabClient, threadClient) {
                       gThreadClient2 = threadClient;
                       test_nesting();
                     });
  });
}

function test_nesting() {
  gThreadClient1.resume(response => {
    Assert.equal(response.error, "wrongOrder");
    gThreadClient2.resume(response => {
      Assert.ok(!response.error);
      Assert.equal(response.from, gThreadClient2.actor);

      gThreadClient1.resume(response => {
        Assert.ok(!response.error);
        Assert.equal(response.from, gThreadClient1.actor);

        gClient1.close(() => finishClient(gClient2));
      });
    });
  });
}
