/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the disablechrome attribute gets propogated to the main UI

const HTTPSRC = "http://example.com/browser/browser/base/content/test/";

function is_element_hidden(aElement) {
  var style = window.getComputedStyle(document.getElementById("nav-bar"), "");
  if (style.visibility != "visible" || style.display == "none")
    return true;

  if (aElement.ownerDocument != aElement.parentNode)
    return is_element_hidden(aElement.parentNode);

  return false;
}

function is_chrome_hidden() {
  is(document.documentElement.getAttribute("disablechrome"), "true", "Attribute should be set");
  if (TabsOnTop.enabled)
    ok(is_element_hidden(document.getElementById("nav-bar")), "Toolbar should be hidden");
  else
    ok(!is_element_hidden(document.getElementById("nav-bar")), "Toolbar should not be hidden");
}

function is_chrome_visible() {
  isnot(document.getElementById("main-window").getAttribute("disablechrome"), "true", "Attribute should not be set");
  ok(!is_element_hidden(document.getElementById("nav-bar")), "Toolbar should not be hidden");
}

function load_page(aURL, aCanHide, aCallback) {
  gNewBrowser.addEventListener("pageshow", function() {
    // Filter out about:blank loads
    if (gNewBrowser.currentURI.spec != aURL)
      return;

    gNewBrowser.removeEventListener("pageshow", arguments.callee, false);

    if (aCanHide)
      is_chrome_hidden();
    else
      is_chrome_visible();

    if (aURL == "about:addons") {
      function check_after_init() {
        if (aCanHide)
          is_chrome_hidden();
        else
          is_chrome_visible();

        aCallback();
      }

      if (gNewBrowser.contentWindow.gIsInitializing) {
        gNewBrowser.contentDocument.addEventListener("Initialized", function() {
          gNewBrowser.contentDocument.removeEventListener("Initialized", arguments.callee, false);

          check_after_init();
        }, false);
      }
      else {
        check_after_init();
      }
    }
    else {
      executeSoon(aCallback);
    }
  }, false);
  gNewBrowser.loadURI(aURL);
}

var gOldTab;
var gNewTab;
var gNewBrowser;

function test() {
  // Opening the add-ons manager and waiting for it to load the discovery pane
  // takes more time in windows debug builds
  requestLongerTimeout(2);

  var gOldTabsOnTop = TabsOnTop.enabled;
  registerCleanupFunction(function() {
    TabsOnTop.enabled = gOldTabsOnTop;
  });

  waitForExplicitFinish();

  gOldTab = gBrowser.selectedTab;
  gNewTab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  gNewBrowser = gBrowser.selectedBrowser;

  info("Tabs on top");
  TabsOnTop.enabled = true;

  run_http_test_1();
}

function end_test() {
  gBrowser.removeTab(gNewTab);
  finish();
}

function test_url(aURL, aCanHide, aNextTest) {
  is_chrome_visible();
  info("Page load");
  load_page(aURL, aCanHide, function() {
    info("Switch away");
    gBrowser.selectedTab = gOldTab;
    is_chrome_visible();

    info("Switch back");
    gBrowser.selectedTab = gNewTab;
    if (aCanHide)
      is_chrome_hidden();
    else
      is_chrome_visible();

    gBrowser.removeTab(gNewTab);
    gNewTab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
    gNewBrowser = gBrowser.selectedBrowser;

    gBrowser.selectedTab = gOldTab;

    info("Background load");
    load_page(aURL, false, function() {
      info("Switch back");
      gBrowser.selectedTab = gNewTab;
      if (aCanHide)
        is_chrome_hidden();
      else
        is_chrome_visible();

      load_page("about:blank", false, aNextTest);
    });
  });
}

// Should never hide the chrome
function run_http_test_1() {
  info("HTTP tests");
  test_url(HTTPSRC + "disablechrome.html", false, run_chrome_about_test);
}

// Should hide the chrome
function run_chrome_about_test() {
  info("Chrome about: tests");
  test_url("about:addons", true, function() {
    info("Tabs on bottom");
    TabsOnTop.enabled = false;
    run_http_test_2();
  });
}

// Should never hide the chrome
function run_http_test_2() {
  info("HTTP tests");
  test_url(HTTPSRC + "disablechrome.html", false, run_chrome_about_test_2);
}

// Should not hide the chrome
function run_chrome_about_test_2() {
  info("Chrome about: tests");
  test_url("about:addons", true, run_http_test3);
}

function run_http_test3() {
  info("HTTP tests");
  test_url(HTTPSRC + "disablechrome.html", false, run_chrome_about_test_3);
}

// Should not hide the chrome
function run_chrome_about_test_3() {
  info("Chrome about: tests");
  test_url("about:Addons", true, function(){
    info("Tabs on top");
    TabsOnTop.enabled = true;
    run_http_test4();
  });
}

function run_http_test4() {
  info("HTTP tests");
  test_url(HTTPSRC + "disablechrome.html", false, run_chrome_about_test_4);
}

function run_chrome_about_test_4() {
  info("Chrome about: tests");
  test_url("about:Addons", true, run_http_test5);
 }

function run_http_test5() {
  info("HTTP tests");
  test_url(HTTPSRC + "disablechrome.html", false, run_chrome_about_test_5);
}

// Should hide the chrome
function run_chrome_about_test_5() {
  info("Chrome about: tests");
  test_url("about:preferences", true, function(){
    info("Tabs on bottom");
    TabsOnTop.enabled = false;
    run_http_test6();
  });
}

function run_http_test6() {
  info("HTTP tests");
  test_url(HTTPSRC + "disablechrome.html", false, run_chrome_about_test_6);
}

function run_chrome_about_test_6() {
  info("Chrome about: tests");
  test_url("about:preferences", true, end_test);
}