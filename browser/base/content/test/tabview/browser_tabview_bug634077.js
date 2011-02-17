/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let ss = Cc["@mozilla.org/browser/sessionstore;1"]
                     .getService(Ci.nsISessionStore);

  let assertVisibilityDataEquals = function (val, msg) {
    is(ss.getWindowValue(window, TabView.VISIBILITY_IDENTIFIER), val, msg);
  }

  waitForExplicitFinish();

  ss.setWindowValue(window, window.TabView.VISIBILITY_IDENTIFIER, "null");
  assertVisibilityDataEquals("null", "we start with <null>");

  showTabView(function () {
    assertVisibilityDataEquals("true", "after showing visibility data is <true>");

    hideTabView(function () {
      assertVisibilityDataEquals("false", "after hiding visibility data is <false>");
      finish();
    });
  });
}
