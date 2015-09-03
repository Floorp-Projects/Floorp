/*
 * Description of the Tests for
 *  - Bug 906190 - Persist "disable protection" option for Mixed Content Blocker in child tabs
 *
 * 1. Open a page from the same domain in a child tab
 *    - Load a html page which has mixed content
 *    - Doorhanger to disable protection appears - we disable it
 *    - Load a subpage from the same origin in a new tab simulating a click
 *    - Doorhanger should >> NOT << appear anymore!
 *
 * 2. Open a page from a different domain in a child tab
 *    - Load a html page which has mixed content
 *    - Doorhanger to disable protection appears - we disable it
 *    - Load a new page from a different origin in a new tab simulating a click
 *    - Doorhanger >> SHOULD << appear again!
 *
 * 3. [meta-refresh: same origin] Open a page from the same domain in a child tab
 *    - Load a html page which has mixed content
 *    - Doorhanger to disable protection appears - we disable it
 *    - Load a new page from the same origin in a new tab simulating a click
 *    - Redirect that page to another page from the same origin using meta-refresh
 *    - Doorhanger should >> NOT << appear again!
 *
 * 4. [meta-refresh: different origin] Open a page from a different domain in a child tab
 *    - Load a html page which has mixed content
 *    - Doorhanger to disable protection appears - we disable it
 *    - Load a new page from the same origin in a new tab simulating a click
 *    - Redirect that page to another page from a different origin using meta-refresh
 *    - Doorhanger >> SHOULD << appear again!
 *
 * 5. [302 redirect: same origin] Open a page from the same domain in a child tab
 *    - Load a html page which has mixed content
 *    - Doorhanger to disable protection appears - we disable it
 *    - Load a new page from the same origin in a new tab simulating a click
 *    - Redirect that page to another page from the same origin using 302 redirect
 *    - Doorhanger >> APPEARS << , but should >> NOT << appear again!
 *    - FOLLOW UP BUG 914860!
 *
 * 6. [302 redirect: different origin] Open a page from the same domain in a child tab
 *    - Load a html page which has mixed content
 *    - Doorhanger to disable protection appears - we disable it
 *    - Load a new page from the same origin in a new tab simulating a click
 *    - Redirect that page to another page from a different origin using 302 redirect
 *    - Doorhanger >> SHOULD << appear again!
 *
 * Note, for all tests we set gHttpTestRoot to use 'https' and we test both,
 *   - |CTRL-CLICK|, as well as
 *   - |RIGHT-CLICK->OPEN LINK IN TAB|.
 */

const PREF_ACTIVE = "security.mixed_content.block_active_content";

// We use the different urls for testing same origin checks before allowing
// mixed content on child tabs.
const gHttpTestRoot1 = "https://test1.example.com/browser/browser/base/content/test/general/";
const gHttpTestRoot2 = "https://test2.example.com/browser/browser/base/content/test/general/";

let origBlockActive;
let gTestWin = null;
let mainTab = null;
let curClickHandler = null;
let curContextMenu = null;
let curTestFunc = null;
let curTestName = null;
let curChildTabLink = null;

//------------------------ Helper Functions ---------------------

registerCleanupFunction(function() {
  // Set preferences back to their original values
  Services.prefs.setBoolPref(PREF_ACTIVE, origBlockActive);
});
requestLongerTimeout(2);

/*
 * Whenever we disable the Mixed Content Blocker of the page
 * we have to make sure that our condition is properly loaded.
 */
function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    if (condition()) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() {
    clearInterval(interval); nextTest();
  };
}

// The purpose of this function is to simulate |CTRL+CLICK|.
// The clickHandler intercepts simulated user clicks and performs
// the |contentAreaClick| which dispatches to handleLinkClick.
let clickHandler = function (aEvent, aFunc) {
  gTestWin.gBrowser.removeEventListener("click", curClickHandler, true);
  gTestWin.contentAreaClick(aEvent, true);
  waitForSomeTabToLoad(aFunc);
  aEvent.preventDefault();
  aEvent.stopPropagation();
}

// The purpose of this function is to simulate |RIGHT-CLICK|->|OPEN LINK IN TAB|
// Once the contextmenu opens, this functions selects 'open link in tab'
// from the contextmenu which dispatches to the function openLinkInTab.
let contextMenuOpenHandler = function(aEvent, aFunc) {
  gTestWin.document.removeEventListener("popupshown", curContextMenu, false);
  waitForSomeTabToLoad(aFunc);
  var openLinkInTabCommand = gTestWin.document.getElementById("context-openlinkintab");
  openLinkInTabCommand.doCommand();
  aEvent.target.hidePopup();
};

function setUpTest(aTestName, aIDForNextTest, aFuncForNextTest, aChildTabLink) {
  curTestName = aTestName;
  curTestFunc = aFuncForNextTest;
  curChildTabLink = aChildTabLink;

  mainTab = gTestWin.gBrowser.selectedTab;
  // get the link for the next test from the main page
  let target = gTestWin.content.document.getElementById(aIDForNextTest).href;
  gTestWin.gBrowser.addTab(target);
  gTestWin.gBrowser.selectTabAtIndex(1);
  waitForSomeTabToLoad(checkPopUpNotification);
}

// Waits for a load event somewhere in the browser but ignore events coming
// from <xul:browser>s without a tab assigned. That are most likely browsers
// that preload the new tab page.
function waitForSomeTabToLoad(callback) {
  gTestWin.gBrowser.addEventListener("load", function onLoad(event) {
    let tab = gTestWin.gBrowser._getTabForContentWindow(event.target.defaultView.top);
    if (tab) {
      gTestWin.gBrowser.removeEventListener("load", onLoad, true);
      callback();
    }
  }, true);
}

function checkPopUpNotification() {
  waitForSomeTabToLoad(reloadedTabAfterDisablingMCB);

  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  // Disable Mixed Content Protection for the page (which reloads the page)
  let {gIdentityHandler} = gTestWin.gBrowser.ownerGlobal;
  gIdentityHandler.disableMixedContentProtection();
}

function reloadedTabAfterDisablingMCB() {
  var expected = "Mixed Content Blocker disabled";
  waitForCondition(
    function() gTestWin.content.document.getElementById('mctestdiv').innerHTML == expected,
    makeSureMCBisDisabled, "Error: Waited too long for mixed script to run in " + curTestName + "!");
}

function makeSureMCBisDisabled() {
  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Made sure MCB is disabled in " + curTestName + "!");

  // inject the provided link into the page, so we can test persistence of MCB
  let doc = gTestWin.content.document;
  let mainDiv = gTestWin.content.document.createElement("div");
  mainDiv.innerHTML =
    '<p><a id="' + curTestName + '" href="' + curChildTabLink + '">' +
    curTestName + '</a></p>';
  doc.body.appendChild(mainDiv);

  curTestFunc();
}

//------------------------ Test 1 ------------------------------

function test1() {
  curClickHandler = function (e) { clickHandler(e, test1A) };
  gTestWin.gBrowser.addEventListener("click", curClickHandler , true);

  // simulating |CTRL-CLICK|
  let targetElt = gTestWin.content.document.getElementById("Test1");
  EventUtils.synthesizeMouseAtCenter(targetElt, { button: 1 }, gTestWin.content);
}

function test1A() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear but isMixedContentBlocked should be >> NOT << true,
  // because our decision of disabling the mixed content blocker is persistent across tabs.
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: true, activeBlocked: false, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 1A");

  gTestWin.gBrowser.removeCurrentTab();
  test1B();
}

function test1B() {
  curContextMenu = function (e) { contextMenuOpenHandler(e, test1C) };
  gTestWin.document.addEventListener("popupshown", curContextMenu, false);

  // simulating |RIGHT-CLICK -> OPEN LINK IN TAB|
  let targetElt = gTestWin.content.document.getElementById("Test1");
  // button 2 is a right click, hence the context menu pops up
  EventUtils.synthesizeMouseAtCenter(targetElt, { type : "contextmenu", button : 2 } , gTestWin.content);
}

function test1C() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear but isMixedContentBlocked should be >> NOT << true,
  // because our decision of disabling the mixed content blocker is persistent across tabs.
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: true, activeBlocked: false, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 1C");

  // remove tabs
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[2], {animate: false});
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[1], {animate: false});
  gTestWin.gBrowser.selectTabAtIndex(0);

  var childTabLink = gHttpTestRoot2 + "file_bug906190_2.html";
  setUpTest("Test2", "linkForTest2", test2, childTabLink);
}

//------------------------ Test 2 ------------------------------

function test2() {
  curClickHandler = function (e) { clickHandler(e, test2A) };
  gTestWin.gBrowser.addEventListener("click", curClickHandler, true);

  // simulating |CTRL-CLICK|
  let targetElt = gTestWin.content.document.getElementById("Test2");
  EventUtils.synthesizeMouseAtCenter(targetElt, { button: 1 }, gTestWin.content);
}

function test2A() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear and isMixedContentBlocked should be >> TRUE <<,
  // because our decision of disabling the mixed content blocker should only persist if pages are from the same domain.
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker enabled", "OK: Blocked mixed script in Test 2A");

  gTestWin.gBrowser.removeCurrentTab();
  test2B();
}

function test2B() {
  curContextMenu = function (e) { contextMenuOpenHandler(e, test2C) };
  gTestWin.document.addEventListener("popupshown", curContextMenu, false);

  // simulating |RIGHT-CLICK -> OPEN LINK IN TAB|
  let targetElt = gTestWin.content.document.getElementById("Test2");
  // button 2 is a right click, hence the context menu pops up
  EventUtils.synthesizeMouseAtCenter(targetElt, { type : "contextmenu", button : 2 } , gTestWin.content);
}

function test2C() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear and isMixedContentBlocked should be >> TRUE <<,
  // because our decision of disabling the mixed content blocker should only persist if pages are from the same domain.
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker enabled", "OK: Blocked mixed script in Test 2C");

  // remove tabs
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[2], {animate: false});
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[1], {animate: false});
  gTestWin.gBrowser.selectTabAtIndex(0);

  // file_bug906190_3_4.html redirects to page test1.example.com/* using meta-refresh
  var childTabLink = gHttpTestRoot1 + "file_bug906190_3_4.html";
  setUpTest("Test3", "linkForTest3", test3, childTabLink);
}

//------------------------ Test 3 ------------------------------

function test3() {
  curClickHandler = function (e) { clickHandler(e, test3A) };
  gTestWin.gBrowser.addEventListener("click", curClickHandler, true);
  // simulating |CTRL-CLICK|
  let targetElt = gTestWin.content.document.getElementById("Test3");
  EventUtils.synthesizeMouseAtCenter(targetElt, { button: 1 }, gTestWin.content);
}

function test3A() {
  // we need this indirection because the page is reloaded caused by meta-refresh
  waitForSomeTabToLoad(test3B);
}

function test3B() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear but isMixedContentBlocked should be >> NOT << true!
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: true, activeBlocked: false, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 3B");

  // remove tabs
  gTestWin.gBrowser.removeCurrentTab();
  test3C();
}

function test3C() {
  curContextMenu = function (e) { contextMenuOpenHandler(e, test3D) };
  gTestWin.document.addEventListener("popupshown", curContextMenu, false);

  // simulating |RIGHT-CLICK -> OPEN LINK IN TAB|
  let targetElt = gTestWin.content.document.getElementById("Test3");
  EventUtils.synthesizeMouseAtCenter(targetElt, { type : "contextmenu", button : 2 } , gTestWin.content);
}

function test3D() {
  // we need this indirection because the page is reloaded caused by meta-refresh
  waitForSomeTabToLoad(test3E);
}

function test3E() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear but isMixedContentBlocked should be >> NOT << true!
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: true, activeBlocked: false, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 3E");

  // remove tabs
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[2], {animate: false});
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[1], {animate: false});
  gTestWin.gBrowser.selectTabAtIndex(0);

  var childTabLink = gHttpTestRoot1 + "file_bug906190_3_4.html";
  setUpTest("Test4", "linkForTest4", test4, childTabLink);
}

//------------------------ Test 4 ------------------------------

function test4() {
  curClickHandler = function (e) { clickHandler(e, test4A) };
  gTestWin.gBrowser.addEventListener("click", curClickHandler, true);

  // simulating |CTRL-CLICK|
  let targetElt = gTestWin.content.document.getElementById("Test4");
  EventUtils.synthesizeMouseAtCenter(targetElt, { button: 1 }, gTestWin.content);
}

function test4A() {
  // we need this indirection because the page is reloaded caused by meta-refresh
  waitForSomeTabToLoad(test4B);
}

function test4B() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear and isMixedContentBlocked should be >> TRUE <<
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker enabled", "OK: Blocked mixed script in Test 4B");

  // remove tabs
  gTestWin.gBrowser.removeCurrentTab();
  test4C();
}

function test4C() {
  curContextMenu = function (e) { contextMenuOpenHandler(e, test4D) };
  gTestWin.document.addEventListener("popupshown", curContextMenu, false);

  // simulating |RIGHT-CLICK -> OPEN LINK IN TAB|
  let targetElt = gTestWin.content.document.getElementById("Test4");
  EventUtils.synthesizeMouseAtCenter(targetElt, { type : "contextmenu", button : 2 } , gTestWin.content);
}

function test4D() {
  // we need this indirection because the page is reloaded caused by meta-refresh
  waitForSomeTabToLoad(test4E);
}

function test4E() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear and isMixedContentBlocked should be >> TRUE <<
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker enabled", "OK: Blocked mixed script in Test 4E");

  // remove tabs
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[2], {animate: false});
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[1], {animate: false});
  gTestWin.gBrowser.selectTabAtIndex(0);

  // the sjs files returns a 302 redirect- note, same origins
  var childTabLink = gHttpTestRoot1 + "file_bug906190.sjs";
  setUpTest("Test5", "linkForTest5", test5, childTabLink);
}

//------------------------ Test 5 ------------------------------

function test5() {
  curClickHandler = function (e) { clickHandler(e, test5A) };
  gTestWin.gBrowser.addEventListener("click", curClickHandler, true);

  // simulating |CTRL-CLICK|
  let targetElt = gTestWin.content.document.getElementById("Test5");
  EventUtils.synthesizeMouseAtCenter(targetElt, { button: 1 }, gTestWin.content);
}

function test5A() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear but isMixedContentBlocked should be >> NOT << true
  // Currently it is >> TRUE << - see follow up bug 914860
  let {gIdentityHandler} = gTestWin.gBrowser.ownerGlobal;
  todo(!gIdentityHandler._identityBox.classList.contains("mixedActiveBlocked"), "OK: Mixed Content is NOT being blocked in Test 5A!");

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  todo_is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 5A!");

  // remove tabs
  gTestWin.gBrowser.removeCurrentTab();
  test5B();
}

function test5B() {
  curContextMenu = function (e) { contextMenuOpenHandler(e, test5C) };
  gTestWin.document.addEventListener("popupshown", curContextMenu, false);

  // simulating |RIGHT-CLICK -> OPEN LINK IN TAB|
  let targetElt = gTestWin.content.document.getElementById("Test5");
  EventUtils.synthesizeMouseAtCenter(targetElt, { type : "contextmenu", button : 2 } , gTestWin.content);
}

function test5C() {
  // move the tab again
  gTestWin.gBrowser.selectTabAtIndex(2);

  let {gIdentityHandler} = gTestWin.gBrowser.ownerGlobal;

  // The Doorhanger should appear but isMixedContentBlocked should be >> NOT << true
  // Currently it is >> TRUE << - see follow up bug 914860
  todo(!gIdentityHandler._identityBox.classList.contains("mixedActiveBlocked"), "OK: Mixed Content is NOT being blocked in Test 5C!");

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  todo_is(actual, "Mixed Content Blocker disabled", "OK: Executed mixed script in Test 5C!");

  // remove tabs
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[2], {animate: false});
  gTestWin.gBrowser.removeTab(gTestWin.gBrowser.tabs[1], {animate: false});
  gTestWin.gBrowser.selectTabAtIndex(0);

  // the sjs files returns a 302 redirect - note, different origins
  var childTabLink = gHttpTestRoot2 + "file_bug906190.sjs";
  setUpTest("Test6", "linkForTest6", test6, childTabLink);
}

//------------------------ Test 6 ------------------------------

function test6() {
  curClickHandler = function (e) { clickHandler(e, test6A) };
  gTestWin.gBrowser.addEventListener("click", curClickHandler, true);

  // simulating |CTRL-CLICK|
  let targetElt = gTestWin.content.document.getElementById("Test6");
  EventUtils.synthesizeMouseAtCenter(targetElt, { button: 1 }, gTestWin.content);
}

function test6A() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear and isMixedContentBlocked should be >> TRUE <<
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker enabled", "OK: Blocked mixed script in Test 6A");

  // done
  gTestWin.gBrowser.removeCurrentTab();
  test6B();
}

function test6B() {
  curContextMenu = function (e) { contextMenuOpenHandler(e, test6C) };
  gTestWin.document.addEventListener("popupshown", curContextMenu, false);

  // simulating |RIGHT-CLICK -> OPEN LINK IN TAB|
  let targetElt = gTestWin.content.document.getElementById("Test6");
  EventUtils.synthesizeMouseAtCenter(targetElt, { type : "contextmenu", button : 2 } , gTestWin.content);
}

function test6C() {
  gTestWin.gBrowser.selectTabAtIndex(2);

  // The Doorhanger should appear and isMixedContentBlocked should be >> TRUE <<
  assertMixedContentBlockingState(gTestWin.gBrowser, {activeLoaded: false, activeBlocked: true, passiveLoaded: false});

  var actual = gTestWin.content.document.getElementById('mctestdiv').innerHTML;
  is(actual, "Mixed Content Blocker enabled", "OK: Blocked mixed script in Test 6C");

  gTestWin.close();
  finish();
}

//------------------------ SETUP ------------------------------

function setupTestBrowserWindow() {
  // Inject links in content.
  let doc = gTestWin.content.document;
  let mainDiv = doc.createElement("div");
  mainDiv.innerHTML =
    '<p><a id="linkForTest1" href="'+ gHttpTestRoot1 + 'file_bug906190_1.html">Test 1</a></p>' +
    '<p><a id="linkForTest2" href="'+ gHttpTestRoot1 + 'file_bug906190_2.html">Test 2</a></p>' +
    '<p><a id="linkForTest3" href="'+ gHttpTestRoot1 + 'file_bug906190_1.html">Test 3</a></p>' +
    '<p><a id="linkForTest4" href="'+ gHttpTestRoot2 + 'file_bug906190_1.html">Test 4</a></p>' +
    '<p><a id="linkForTest5" href="'+ gHttpTestRoot1 + 'file_bug906190_1.html">Test 5</a></p>' +
    '<p><a id="linkForTest6" href="'+ gHttpTestRoot1 + 'file_bug906190_1.html">Test 6</a></p>';
  doc.body.appendChild(mainDiv);
}

function startTests() {
  mainTab = gTestWin.gBrowser.selectedTab;
  var childTabLink = gHttpTestRoot1 + "file_bug906190_2.html";
  setUpTest("Test1", "linkForTest1", test1, childTabLink);
}

function test() {
  // Performing async calls, e.g. 'onload', we have to wait till all of them finished
  waitForExplicitFinish();

  // Store original preferences so we can restore settings after testing
  origBlockActive = Services.prefs.getBoolPref(PREF_ACTIVE);
  Services.prefs.setBoolPref(PREF_ACTIVE, true);

  gTestWin = openDialog(location, "", "chrome,all,dialog=no", "about:blank");
  whenDelayedStartupFinished(gTestWin, function () {
    info("browser window opened");
    waitForFocus(function() {
      info("browser window focused");
      waitForFocus(function() {
        info("setting up browser...");
        setupTestBrowserWindow();
        info("running tests...");
        executeSoon(startTests);
      }, gTestWin.content, true);
    }, gTestWin);
  });
}
