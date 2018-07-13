const {Utils} = ChromeUtils.import("resource://gre/modules/sessionstore/Utils.jsm", {});
const triggeringPrincipal_base64 = Utils.SERIALIZED_SYSTEMPRINCIPAL;

add_task(async function() {
  waitForExplicitFinish();

  const tabURL = getRootDirectory(gTestPath) + "browser_bug1184989_prevent_scrolling_when_preferences_flipped.xul";

  await BrowserTestUtils.withNewTab({ gBrowser, url: tabURL }, async function(browser) {
    let doc = browser.contentDocument;
    let container = doc.getElementById("container");

    // Test button
    let button = doc.getElementById("button");
    button.focus();
    EventUtils.sendString(" ");
    await checkPageScrolling(container, "button");

    // Test checkbox
    let checkbox = doc.getElementById("checkbox");
    checkbox.focus();
    EventUtils.sendString(" ");
    ok(checkbox.checked, "Checkbox is checked");
    await checkPageScrolling(container, "checkbox");

    // Test radio
    let radiogroup = doc.getElementById("radiogroup");
    radiogroup.focus();
    EventUtils.sendString(" ");
    await checkPageScrolling(container, "radio");
  });

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:preferences#search" }, async function(browser) {
    let doc = browser.contentDocument;
    let container = doc.getElementsByClassName("main-content")[0];

    // Test search
    let engineList = doc.getElementById("engineList");
    engineList.focus();
    EventUtils.sendString(" ");
    is(engineList.view.selection.currentIndex, 0, "Search engineList is selected");
    EventUtils.sendString(" ");
    await checkPageScrolling(container, "search engineList");
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

  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank");

  // Fake a post-crash tab
  SessionStore.setTabState(tab, JSON.stringify(TAB_STATE));

  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  let doc = tab.linkedBrowser.contentDocument;

  // Make body scrollable
  doc.body.style.height = (doc.body.clientHeight + 100) + "px";

  let tabsToggle = doc.getElementById("tabsToggle");
  tabsToggle.focus();
  EventUtils.sendString(" ");
  await checkPageScrolling(doc.documentElement, "session restore");

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
