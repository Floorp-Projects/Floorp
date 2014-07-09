/*
 * User Override Mixed Content Block - Tests for Bug 822367
 */


const PREF_DISPLAY = "security.mixed_content.block_display_content";
const PREF_ACTIVE = "security.mixed_content.block_active_content";

// We alternate for even and odd test cases to simulate different hosts
const gHttpTestRoot = "https://example.com/browser/browser/base/content/test/general/";
const gHttpTestRoot2 = "https://test1.example.com/browser/browser/base/content/test/general/";

var origBlockDisplay;
var origBlockActive;
var gTestBrowser = null;

registerCleanupFunction(function() {
  // Set preferences back to their original values
  Services.prefs.setBoolPref(PREF_DISPLAY, origBlockDisplay);
  Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
});

function MixedTestsCompleted() {
  gBrowser.removeCurrentTab();
  window.focus();
  finish();
}

function test() {
  waitForExplicitFinish();

  origBlockDisplay = Services.prefs.getBoolPref(PREF_DISPLAY);
  origBlockActive = Services.prefs.getBoolPref(PREF_ACTIVE);

  Services.prefs.setBoolPref(PREF_DISPLAY, true);
  Services.prefs.setBoolPref(PREF_ACTIVE, true);

  var newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  gTestBrowser = gBrowser.selectedBrowser;
  newTab.linkedBrowser.stop()

  // Mixed Script Test
  gTestBrowser.addEventListener("load", MixedTest1A, true);
  var url = gHttpTestRoot + "file_bug822367_1.html";
  gTestBrowser.contentWindow.location = url;
}

// Mixed Script Test
function MixedTest1A() {
  gTestBrowser.removeEventListener("load", MixedTest1A, true);
  gTestBrowser.addEventListener("load", MixedTest1B, true);
  var notification = PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser);
  ok(notification, "Mixed Content Doorhanger didn't appear");
  notification.secondaryActions[0].callback();
}
function MixedTest1B() {
  waitForCondition(function() content.document.getElementById('p1').innerHTML == "hello", MixedTest1C, "Waited too long for mixed script to run in Test 1");
}
function MixedTest1C() {
  ok(content.document.getElementById('p1').innerHTML == "hello","Mixed script didn't load in Test 1");
  gTestBrowser.removeEventListener("load", MixedTest1B, true);
  MixedTest2();
}

//Mixed Display Test - Doorhanger should not appear
function MixedTest2() {
  gTestBrowser.addEventListener("load", MixedTest2A, true);
  var url = gHttpTestRoot2 + "file_bug822367_2.html";
  gTestBrowser.contentWindow.location = url;
}

function MixedTest2A() {
  var notification = PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser);
  ok(!notification, "Mixed Content Doorhanger appears for mixed display content!");
  MixedTest3();
}

// Mixed Script and Display Test - User Override should cause both the script and the image to load.
function MixedTest3() {
  gTestBrowser.removeEventListener("load", MixedTest2A, true);
  gTestBrowser.addEventListener("load", MixedTest3A, true);
  var url = gHttpTestRoot + "file_bug822367_3.html";
  gTestBrowser.contentWindow.location = url;
}
function MixedTest3A() {
  gTestBrowser.removeEventListener("load", MixedTest3A, true);
  gTestBrowser.addEventListener("load", MixedTest3B, true);
  var notification = PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser);
  ok(notification, "Mixed Content Doorhanger doesn't appear for test 3");
  notification.secondaryActions[0].callback();
}
function MixedTest3B() {
  waitForCondition(function() content.document.getElementById('p1').innerHTML == "hello", MixedTest3C, "Waited too long for mixed script to run in Test 3");
}
function MixedTest3C() {
  waitForCondition(function() content.document.getElementById('p2').innerHTML == "bye", MixedTest3D, "Waited too long for mixed image to load in Test 3");
}
function MixedTest3D() {
  ok(content.document.getElementById('p1').innerHTML == "hello","Mixed script didn't load in Test 3");
  ok(content.document.getElementById('p2').innerHTML == "bye","Mixed image didn't load in Test 3");
  MixedTest4();
}

// Location change - User override on one page doesn't propogate to another page after location change.
function MixedTest4() {
  gTestBrowser.removeEventListener("load", MixedTest3B, true);
  gTestBrowser.addEventListener("load", MixedTest4A, true);
  var url = gHttpTestRoot2 + "file_bug822367_4.html";
  gTestBrowser.contentWindow.location = url;
}
function MixedTest4A() {
  gTestBrowser.removeEventListener("load", MixedTest4A, true);
  gTestBrowser.addEventListener("load", MixedTest4B, true);
  var notification = PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser);
  ok(notification, "Mixed Content Doorhanger doesn't appear for Test 4");
  notification.secondaryActions[0].callback();
}
function MixedTest4B() {
  waitForCondition(function() content.document.location == gHttpTestRoot + "file_bug822367_4B.html", MixedTest4C, "Waited too long for mixed script to run in Test 4");
}
function MixedTest4C() {
  ok(content.document.location == gHttpTestRoot + "file_bug822367_4B.html", "Location didn't change in test 4");
  var notification = PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser);
  ok(notification, "Mixed Content Doorhanger doesn't appear after location change in Test 4");
  waitForCondition(function() content.document.getElementById('p1').innerHTML == "", MixedTest4D, "Mixed script loaded in test 4 after location change!");
}
function MixedTest4D() {
  ok(content.document.getElementById('p1').innerHTML == "","p1.innerHTML changed; mixed script loaded after location change in Test 4");
  MixedTest5();
}

// Mixed script attempts to load in a document.open()
function MixedTest5() {
  gTestBrowser.removeEventListener("load", MixedTest4B, true);
  gTestBrowser.addEventListener("load", MixedTest5A, true);
  var url = gHttpTestRoot + "file_bug822367_5.html";
  gTestBrowser.contentWindow.location = url;
}
function MixedTest5A() {
  gTestBrowser.removeEventListener("load", MixedTest5A, true);
  gTestBrowser.addEventListener("load", MixedTest5B, true);
  var notification = PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser);
  ok(notification, "Mixed Content Doorhanger doesn't appear for Test 5");
  notification.secondaryActions[0].callback();
}
function MixedTest5B() {
  waitForCondition(function() content.document.getElementById('p1').innerHTML == "hello", MixedTest5C, "Waited too long for mixed script to run in Test 5");
}
function MixedTest5C() {
  ok(content.document.getElementById('p1').innerHTML == "hello","Mixed script didn't load in Test 5");
  MixedTest6();
}

// Mixed script attempts to load in a document.open() that is within an iframe.
function MixedTest6() {
  gTestBrowser.removeEventListener("load", MixedTest5B, true);
  gTestBrowser.addEventListener("load", MixedTest6A, true);
  var url = gHttpTestRoot2 + "file_bug822367_6.html";
  gTestBrowser.contentWindow.location = url;
}
function MixedTest6A() {
  gTestBrowser.removeEventListener("load", MixedTest6A, true);
  waitForCondition(function() PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser), MixedTest6B, "waited to long for doorhanger");
}

function MixedTest6B() {
  var notification = PopupNotifications.getNotification("mixed-content-blocked", gTestBrowser);
  ok(notification, "Mixed Content Doorhanger doesn't appear for Test 6");
  gTestBrowser.addEventListener("load", MixedTest6C, true);
  notification.secondaryActions[0].callback();
}

function MixedTest6C() {
  gTestBrowser.removeEventListener("load", MixedTest6C, true);
  waitForCondition(function() {
    try {
      return content.document.getElementById('f1').contentDocument.getElementById('p1').innerHTML == "hello";
    } catch (e) {
      return false;
    }
  }, MixedTest6D, "Waited too long for mixed script to run in Test 6");
}
function MixedTest6D() {
  ok(content.document.getElementById('f1').contentDocument.getElementById('p1').innerHTML == "hello","Mixed script didn't load in Test 6");
  MixedTestsCompleted();
}
