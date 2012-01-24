/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that our Promise implementation works properly

let tempScope = {};
Cu.import("resource:///modules/devtools/Promise.jsm", tempScope);
let Promise = tempScope.Promise;

function test() {
  addTab("about:blank", function() {
    info("Starting Promise Tests");
    testBasic();
  });
}

var postResolution;

function testBasic() {
  postResolution = new Promise();
  ok(postResolution.isPromise, "We have a promise");
  ok(!postResolution.isComplete(), "Promise is initially incomplete");
  ok(!postResolution.isResolved(), "Promise is initially unresolved");
  ok(!postResolution.isRejected(), "Promise is initially unrejected");

  // Test resolve() *after* then() in the same context
  var reply = postResolution.then(testPostResolution, fail)
                            .resolve("postResolution");
  is(reply, postResolution, "return this; working ok");
}

var preResolution;

function testPostResolution(data) {
  is(data, "postResolution", "data is postResolution");
  ok(postResolution.isComplete(), "postResolution Promise is complete");
  ok(postResolution.isResolved(), "postResolution Promise is resolved");
  ok(!postResolution.isRejected(), "postResolution Promise is unrejected");

  try {
    info("Expected double resolve error");
    postResolution.resolve("double resolve");
    ok(false, "double resolve");
  }
  catch (ex) {
    // Expected
  }

  // Test resolve() *before* then() in the same context
  preResolution = new Promise();
  var reply = preResolution.resolve("preResolution")
                           .then(testPreResolution, fail);
  is(reply, preResolution, "return this; working ok");
}

var laterResolution;

function testPreResolution(data) {
  is(data, "preResolution", "data is preResolution");
  ok(preResolution.isComplete(), "preResolution Promise is complete");
  ok(preResolution.isResolved(), "preResolution Promise is resolved");
  ok(!preResolution.isRejected(), "preResolution Promise is unrejected");

  // Test resolve() *after* then() in a later context
  laterResolution = new Promise();
  laterResolution.then(testLaterResolution, fail);
  executeSoon(function() {
    laterResolution.resolve("laterResolution");
  });
}

var laterRejection;

function testLaterResolution(data) {
  is(data, "laterResolution", "data is laterResolution");
  ok(laterResolution.isComplete(), "laterResolution Promise is complete");
  ok(laterResolution.isResolved(), "laterResolution Promise is resolved");
  ok(!laterResolution.isRejected(), "laterResolution Promise is unrejected");

  // Test reject() *after* then() in a later context
  laterRejection = new Promise().then(fail, testLaterRejection);
  executeSoon(function() {
    laterRejection.reject("laterRejection");
  });
}

function testLaterRejection(data) {
  is(data, "laterRejection", "data is laterRejection");
  ok(laterRejection.isComplete(), "laterRejection Promise is complete");
  ok(!laterRejection.isResolved(), "laterRejection Promise is unresolved");
  ok(laterRejection.isRejected(), "laterRejection Promise is rejected");

  // Test chaining
  var orig = new Promise();
  orig.chainPromise(function(data) {
    is(data, "origData", "data is origData");
    return data.replace(/orig/, "new");
  }).then(function(data) {
    is(data, "newData", "data is newData");
    testChain();
  });
  orig.resolve("origData");
}

var member1;
var member2;
var member3;
var laterGroup;

function testChain() {
  // Test an empty group
  var empty1 = Promise.group();
  ok(empty1.isComplete(), "empty1 Promise is complete");
  ok(empty1.isResolved(), "empty1 Promise is resolved");
  ok(!empty1.isRejected(), "empty1 Promise is unrejected");

  // Test a group with no members
  var empty2 = Promise.group([]);
  ok(empty2.isComplete(), "empty2 Promise is complete");
  ok(empty2.isResolved(), "empty2 Promise is resolved");
  ok(!empty2.isRejected(), "empty2 Promise is unrejected");

  // Test grouping using resolve() in a later context
  member1 = new Promise();
  member2 = new Promise();
  member3 = new Promise();
  laterGroup = Promise.group(member1, member2, member3);
  laterGroup.then(testLaterGroup, fail);

  member1.then(function(data) {
    is(data, "member1", "member1 is member1");
    executeSoon(function() {
      member2.resolve("member2");
    });
  }, fail);
  member2.then(function(data) {
    is(data, "member2", "member2 is member2");
    executeSoon(function() {
      member3.resolve("member3");
    });
  }, fail);
  member3.then(function(data) {
    is(data, "member3", "member3 is member3");
    // The group should now fire
  }, fail);
  executeSoon(function() {
    member1.resolve("member1");
  });
}

var tidyGroup;

function testLaterGroup(data) {
  is(data[0], "member1", "member1 is member1");
  is(data[1], "member2", "member2 is member2");
  is(data[2], "member3", "member3 is member3");
  is(data.length, 3, "data.length is right");
  ok(laterGroup.isComplete(), "laterGroup Promise is complete");
  ok(laterGroup.isResolved(), "laterGroup Promise is resolved");
  ok(!laterGroup.isRejected(), "laterGroup Promise is unrejected");

  // Test grouping resolve() *before* then() in the same context
  tidyGroup = Promise.group([
    postResolution, preResolution, laterResolution,
    member1, member2, member3, laterGroup
  ]);
  tidyGroup.then(testTidyGroup, fail);
}

var failGroup;

function testTidyGroup(data) {
  is(data[0], "postResolution", "postResolution is postResolution");
  is(data[1], "preResolution", "preResolution is preResolution");
  is(data[2], "laterResolution", "laterResolution is laterResolution");
  is(data[3], "member1", "member1 is member1");
  is(data[6][1], "member2", "laterGroup is laterGroup");
  is(data.length, 7, "data.length is right");
  ok(tidyGroup.isComplete(), "tidyGroup Promise is complete");
  ok(tidyGroup.isResolved(), "tidyGroup Promise is resolved");
  ok(!tidyGroup.isRejected(), "tidyGroup Promise is unrejected");

  // Test grouping resolve() *before* then() in the same context
  failGroup = Promise.group(postResolution, laterRejection);
  failGroup.then(fail, testFailGroup);
}

function testFailGroup(data) {
  is(data, "laterRejection", "laterRejection is laterRejection");

  postResolution = undefined;
  preResolution = undefined;
  laterResolution = undefined;
  member1 = undefined;
  member2 = undefined;
  member3 = undefined;
  laterGroup = undefined;
  laterRejection = undefined;

  finished();
}

function fail() {
  gBrowser.removeCurrentTab();
  info("Failed Promise Tests");
  ok(false, "fail called");
  finish();
}

function finished() {
  gBrowser.removeCurrentTab();
  info("Finishing Promise Tests");
  finish();
}
