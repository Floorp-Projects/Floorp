/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let tab = gBrowser.addTrustedTab("about:blank", {index: 10});
  is(tab._tPos, 1, "added tab index should be 1");
  gBrowser.removeTab(tab);
}
