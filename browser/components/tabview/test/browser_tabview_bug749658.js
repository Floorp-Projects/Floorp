/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
let win;

function test() {
  waitForExplicitFinish();

  newWindowWithTabView(function(newWin) {
    win = newWin;

    registerCleanupFunction(function() {
      win.close();
    });

    is(win.gBrowser.tabContainer.getAttribute("overflow"), "",
       "The tabstrip should not be overflowing");

    finish();
  }, function(newWin) {
    /* add a tab with a ridiculously long title to trigger bug 749658 */
    var longTitle = "this is a very long title for the new tab ";
    longTitle = longTitle + longTitle + longTitle + longTitle + longTitle;
    longTitle = longTitle + longTitle + longTitle + longTitle + longTitle;
    newWin.gBrowser.addTab("data:text/html,<head><title>" + longTitle);
  });
}
