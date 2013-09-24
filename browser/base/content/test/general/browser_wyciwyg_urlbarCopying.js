/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let url = "http://mochi.test:8888/browser/browser/base/content/test/general/test_wyciwyg_copying.html";
  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  tab.linkedBrowser.addEventListener("pageshow", function () {
    let btn = content.document.getElementById("btn");
    executeSoon(function () {
      EventUtils.synthesizeMouseAtCenter(btn, {}, content);
      let currentURL = gBrowser.currentURI.spec;
      ok(/^wyciwyg:\/\//i.test(currentURL), currentURL + " is a wyciwyg URI");

      executeSoon(function () {
        testURLBarCopy(url, endTest);
      });
    });
  }, false);

  function endTest() {
    while (gBrowser.tabs.length > 1)
      gBrowser.removeCurrentTab();
    finish();
  }

  function testURLBarCopy(targetValue, cb) {
    info("Expecting copy of: " + targetValue);
    waitForClipboard(targetValue, function () {
      gURLBar.focus();
      gURLBar.select();

      goDoCommand("cmd_copy");
    }, cb, cb);
  }
}


