/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

var gTestTab;
var gContentAPI;
var gContentWindow;

function test() {
  UITourTest();
}

var tests = [
  taskify(function* test_closeTab() {
    // Setting gTestTab to null indicates that the tab has already been closed,
    // and if this does not happen the test run will fail.
    gContentAPI.closeTab();
    gTestTab = null;
  }),
];
