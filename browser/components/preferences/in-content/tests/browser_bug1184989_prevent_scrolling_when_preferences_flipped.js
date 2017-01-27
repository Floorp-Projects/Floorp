const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

const {Utils} = Cu.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

add_task(function* () {
  waitForExplicitFinish();

  const tabURL = getRootDirectory(gTestPath) + "browser_bug1184989_prevent_scrolling_when_preferences_flipped.xul";

  yield BrowserTestUtils.withNewTab({ gBrowser, url: tabURL }, function* (browser) {
    let doc = browser.contentDocument;
    let container = doc.getElementById("container");

    // Test button
    let button = doc.getElementById("button");
    button.focus();
    EventUtils.synthesizeKey(" ", {});
    yield checkPageScrolling(container, "button");

    // Test checkbox
    let checkbox = doc.getElementById("checkbox");
    checkbox.focus();
    EventUtils.synthesizeKey(" ", {});
    ok(checkbox.checked, "Checkbox is checked");
    yield checkPageScrolling(container, "checkbox");

    // Test listbox
    let listbox = doc.getElementById("listbox");
    let listitem = doc.getElementById("listitem");
    listbox.focus();
    EventUtils.synthesizeKey(" ", {});
    ok(listitem.selected, "Listitem is selected");
    yield checkPageScrolling(container, "listbox");

    // Test radio
    let radiogroup = doc.getElementById("radiogroup");
    radiogroup.focus();
    EventUtils.synthesizeKey(" ", {});
    yield checkPageScrolling(container, "radio");
  });

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:preferences#search" }, function* (browser) {
    let doc = browser.contentDocument;
    let container = doc.getElementsByClassName("main-content")[0];

    // Test search
    let engineList = doc.getElementById("engineList");
    engineList.focus();
    EventUtils.synthesizeKey(" ", {});
    is(engineList.view.selection.currentIndex, 0, "Search engineList is selected");
    EventUtils.synthesizeKey(" ", {});
    yield checkPageScrolling(container, "search engineList");
  });

  // Test session restore
  const CRASH_URL = "about:mozilla";
  const CRASH_FAVICON = "chrome://branding/content/icon32.png";
  const CRASH_SHENTRY = {url: CRASH_URL};
  const CRASH_TAB = {entries: [CRASH_SHENTRY], image: CRASH_FAVICON};
  const CRASH_STATE = {windows: [{tabs: [CRASH_TAB]}]};

  const TAB_URL = "about:sessionrestore";
  const TAB_FORMDATA = {url: TAB_URL, id: {sessionData: CRASH_STATE}};
  const TAB_SHENTRY = {url: TAB_URL, triggeringPrincipal_base64};
  const TAB_STATE = {entries: [TAB_SHENTRY], formdata: TAB_FORMDATA};

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");

  // Fake a post-crash tab
  ss.setTabState(tab, JSON.stringify(TAB_STATE));

  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  let doc = tab.linkedBrowser.contentDocument;

  // Make body scrollable
  doc.body.style.height = (doc.body.clientHeight + 100) + "px";

  let tabList = doc.getElementById("tabList");
  tabList.focus();
  EventUtils.synthesizeKey(" ", {});
  yield checkPageScrolling(doc.documentElement, "session restore");

  gBrowser.removeCurrentTab();
  finish();
});

function checkPageScrolling(container, type) {
  return new Promise(resolve => {
    setTimeout(() => {
      is(container.scrollTop, 0, "Page should not scroll when " + type + " flipped");
      resolve();
    }, 0);
  });
}
