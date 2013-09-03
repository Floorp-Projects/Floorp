/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let url = "http://mochi.test:8888/browser/browser/devtools/responsivedesign/test/touch.html";

  let mgr = ResponsiveUI.ResponsiveUIManager;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = url;

  function startTest() {
    mgr.once("on", function() {executeSoon(testWithNoTouch)});
    mgr.once("off", function() {executeSoon(finishUp)});
    mgr.toggle(window, gBrowser.selectedTab);
  }

  function testWithNoTouch() {
    let div = content.document.querySelector("div");
    let x = 2, y = 2;
    EventUtils.synthesizeMouse(div, x, y, {type: "mousedown"}, content);
    x += 20; y += 10;
    EventUtils.synthesizeMouse(div, x, y, {type: "mousemove"}, content);
    is(div.style.transform, "", "touch didn't work");
    EventUtils.synthesizeMouse(div, x, y, {type: "mouseup"}, content);
    testWithTouch();
  }

  function testWithTouch() {
    gBrowser.selectedTab.__responsiveUI.enableTouch();
    let div = content.document.querySelector("div");
    let x = 2, y = 2;
    EventUtils.synthesizeMouse(div, x, y, {type: "mousedown"}, content);
    x += 20; y += 10;
    EventUtils.synthesizeMouse(div, x, y, {type: "mousemove"}, content);
    is(div.style.transform, "translate(20px, 10px)", "touch worked");
    EventUtils.synthesizeMouse(div, x, y, {type: "mouseup"}, content);
    is(div.style.transform, "none", "end event worked");
    mgr.toggle(window, gBrowser.selectedTab);
  }

  function testWithTouchAgain() {
    gBrowser.selectedTab.__responsiveUI.disableTouch();
    let div = content.document.querySelector("div");
    let x = 2, y = 2;
    EventUtils.synthesizeMouse(div, x, y, {type: "mousedown"}, content);
    x += 20; y += 10;
    EventUtils.synthesizeMouse(div, x, y, {type: "mousemove"}, content);
    is(div.style.transform, "", "touch didn't work");
    EventUtils.synthesizeMouse(div, x, y, {type: "mouseup"}, content);
    finishUp();
  }


  function finishUp() {
    gBrowser.removeCurrentTab();
    finish();
  }
}
