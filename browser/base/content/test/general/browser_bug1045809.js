// Test that the Mixed Content Doorhanger Action to re-enable protection works

const PREF_ACTIVE = "security.mixed_content.block_active_content";

var origBlockActive;

add_task(function* () {
  registerCleanupFunction(function() {
    Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
    gBrowser.removeCurrentTab();
  });

  // Store original preferences so we can restore settings after testing
  origBlockActive = Services.prefs.getBoolPref(PREF_ACTIVE);

  // Make sure mixed content blocking is on
  Services.prefs.setBoolPref(PREF_ACTIVE, true);

  var url =
    "https://test1.example.com/browser/browser/base/content/test/general/" +
    "file_bug1045809_1.html";
  let tab = gBrowser.selectedTab = gBrowser.addTab();

  // Test 1: mixed content must be blocked
  yield promiseTabLoadEvent(tab, url);
  test1(gBrowser.getBrowserForTab(tab));

  yield promiseTabLoadEvent(tab);
  // Test 2: mixed content must NOT be blocked
  test2(gBrowser.getBrowserForTab(tab));

  // Test 3: mixed content must be blocked again
  yield promiseTabLoadEvent(tab);
  test3(gBrowser.getBrowserForTab(tab));
});

function test1(gTestBrowser) {
  var notification =
    PopupNotifications.getNotification("bad-content", gTestBrowser);
  isnot(notification, null, "Mixed Content Doorhanger did appear in Test1");
  notification.reshow();
  isnot(PopupNotifications.panel.firstChild.isMixedContentBlocked, 0,
    "Mixed Content is being blocked in Test1");

  var x = content.document.getElementsByTagName('iframe')[0].contentDocument.getElementById('mixedContentContainer');
  is(x, null, "Mixed Content is NOT to be found in Test1");

  // Disable Mixed Content Protection for the page (and reload)
  PopupNotifications.panel.firstChild.disableMixedContentProtection();
}

function test2(gTestBrowser) {
  var notification =
    PopupNotifications.getNotification("bad-content", gTestBrowser);
  isnot(notification, null, "Mixed Content Doorhanger did appear in Test2");
  notification.reshow();
  is(PopupNotifications.panel.firstChild.isMixedContentBlocked, 0,
    "Mixed Content is NOT being blocked in Test2");

  var x = content.document.getElementsByTagName('iframe')[0].contentDocument.getElementById('mixedContentContainer');
  isnot(x, null, "Mixed Content is to be found in Test2");

  // Re-enable Mixed Content Protection for the page (and reload)
  PopupNotifications.panel.firstChild.enableMixedContentProtection();
}

function test3(gTestBrowser) {
  var notification =
    PopupNotifications.getNotification("bad-content", gTestBrowser);
  isnot(notification, null, "Mixed Content Doorhanger did appear in Test3");
  notification.reshow();
  isnot(PopupNotifications.panel.firstChild.isMixedContentBlocked, 0,
    "Mixed Content is being blocked in Test3");

  var x = content.document.getElementsByTagName('iframe')[0].contentDocument.getElementById('mixedContentContainer');
  is(x, null, "Mixed Content is NOT to be found in Test3");
}
