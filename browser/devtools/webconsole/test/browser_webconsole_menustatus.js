/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function test() {
  var tab1 = gBrowser.addTab();
  var tab2 = gBrowser.addTab();
  gBrowser.selectedTab = tab2;

  openConsole(tab2, function() {
    let cmd = document.getElementById("Tools:WebConsole");
    is(cmd.getAttribute("checked"), "true", "<command Tools:WebConsole> is checked.");

    gBrowser.selectedTab = tab1;

    is(cmd.getAttribute("checked"), "false", "<command Tools:WebConsole> is unchecked after tab switch.");

    gBrowser.selectedTab = tab2;

    is(cmd.getAttribute("checked"), "true", "<command Tools:WebConsole> is checked.");

    closeConsole(tab2, function() {
      is(cmd.getAttribute("checked"), "false", "<command Tools:WebConsole> is checked once closed.");

      gBrowser.removeTab(tab1);
      gBrowser.removeTab(tab2);

      finish();
    });
  });
}
