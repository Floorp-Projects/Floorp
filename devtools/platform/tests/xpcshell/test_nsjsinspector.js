/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the basic functionality of the nsIJSInspector component.
var gCount = 0;
const MAX = 10;

var inspector = Cc["@mozilla.org/jsinspector;1"].getService(Ci.nsIJSInspector);

// Emulate 10 simultaneously-debugged windows from 3 separate client connections.
var requestor = count => ({
  url: "http://foo/bar/" + count,
  connection: "conn" + (count % 3),
});

function run_test() {
  test_nesting();
}

function test_nesting() {
  Assert.equal(inspector.eventLoopNestLevel, 0);

  Services.tm.dispatchToMainThread({ run: enterEventLoop });

  Assert.equal(inspector.enterNestedEventLoop(requestor(gCount)), 0);
  Assert.equal(inspector.eventLoopNestLevel, 0);
  Assert.equal(inspector.lastNestRequestor, null);
}

function enterEventLoop() {
  if (gCount++ < MAX) {
    Services.tm.dispatchToMainThread({ run: enterEventLoop });

    Object.create(requestor(gCount));

    Assert.equal(inspector.eventLoopNestLevel, gCount);
    Assert.equal(inspector.lastNestRequestor.url, requestor(gCount - 1).url);
    Assert.equal(
      inspector.lastNestRequestor.connection,
      requestor(gCount - 1).connection
    );
    Assert.equal(inspector.enterNestedEventLoop(requestor(gCount)), gCount);
  } else {
    Assert.equal(gCount, MAX + 1);
    Services.tm.dispatchToMainThread({ run: exitEventLoop });
  }
}

function exitEventLoop() {
  if (inspector.lastNestRequestor != null) {
    Assert.equal(inspector.lastNestRequestor.url, requestor(gCount - 1).url);
    Assert.equal(
      inspector.lastNestRequestor.connection,
      requestor(gCount - 1).connection
    );
    if (gCount-- > 1) {
      Services.tm.dispatchToMainThread({ run: exitEventLoop });
    }

    Assert.equal(inspector.exitNestedEventLoop(), gCount);
    Assert.equal(inspector.eventLoopNestLevel, gCount);
  }
}
