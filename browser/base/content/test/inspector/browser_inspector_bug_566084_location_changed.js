/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let notificationBox = null;

function startLocationTests() {
  ok(window.InspectorUI, "InspectorUI variable exists");
  Services.obs.addObserver(runInspectorTests, INSPECTOR_NOTIFICATIONS.OPENED, null);
  InspectorUI.toggleInspectorUI();
}

function runInspectorTests() {
  Services.obs.removeObserver(runInspectorTests, INSPECTOR_NOTIFICATIONS.OPENED, null);

  let para = content.document.querySelector("p");
  ok(para, "found the paragraph element");
  is(para.textContent, "init", "paragraph content is correct");

  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(InspectorUI.isTreePanelOpen, "Inspector Panel is open");

  InspectorUI.isDirty = true;

  notificationBox = gBrowser.getNotificationBox(gBrowser.selectedBrowser);
  notificationBox.addEventListener("AlertActive", alertActive1, false);

  gBrowser.selectedBrowser.addEventListener("load", onPageLoad, true);

  content.location = "data:text/html,<div>location change test 1 for " +
    "inspector</div><p>test1</p>";
}

function alertActive1() {
  notificationBox.removeEventListener("AlertActive", alertActive1, false);

  let notification = notificationBox.
    getNotificationWithValue("inspector-page-navigation");
  ok(notification, "found the inspector-page-navigation notification");

  // By closing the notification it is expected that page navigation is
  // canceled.
  executeSoon(function() {
    notification.close();
    locationTest2();
  });
}

function onPageLoad() {
  gBrowser.selectedBrowser.removeEventListener("load", onPageLoad, true);

  isnot(content.location.href.indexOf("test2"), -1,
        "page navigated to the correct location");

  let para = content.document.querySelector("p");
  ok(para, "found the paragraph element, third time");
  is(para.textContent, "test2", "paragraph content is correct");

  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(!InspectorUI.isTreePanelOpen, "Inspector Panel is not open");

  testEnd();
}

function locationTest2() {
  // Location did not change.
  let para = content.document.querySelector("p");
  ok(para, "found the paragraph element, second time");
  is(para.textContent, "init", "paragraph content is correct");

  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(InspectorUI.isTreePanelOpen, "Inspector Panel is open");

  notificationBox.addEventListener("AlertActive", alertActive2, false);

  content.location = "data:text/html,<div>location change test 2 for " +
    "inspector</div><p>test2</p>";
}

function alertActive2() {
  notificationBox.removeEventListener("AlertActive", alertActive2, false);

  let notification = notificationBox.
    getNotificationWithValue("inspector-page-navigation");
  ok(notification, "found the inspector-page-navigation notification");

  let buttons = notification.querySelectorAll("button");
  let buttonLeave = null;
  for (let i = 0; i < buttons.length; i++) {
    if (buttons[i].buttonInfo.id == "inspector.confirmNavigationAway.buttonLeave") {
      buttonLeave = buttons[i];
      break;
    }
  }

  ok(buttonLeave, "the Leave page button was found");

  // Accept page navigation.
  executeSoon(function(){
    buttonLeave.doCommand();
  });
}

function testEnd() {
  notificationBox = null;
  InspectorUI.isDirty = false;
  gBrowser.removeCurrentTab();
  executeSoon(finish);
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onBrowserLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onBrowserLoad, true);
    waitForFocus(startLocationTests, content);
  }, true);

  content.location = "data:text/html,<div>location change tests for " +
    "inspector.</div><p>init</p>";
}
