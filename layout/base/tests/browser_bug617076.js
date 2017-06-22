/**
 * 1. load about:addons in a new tab and select that tab
 * 2. insert a button with tooltiptext
 * 3. create a new blank tab and select that tab
 * 4. select the about:addons tab and hover the inserted button
 * 5. remove the about:addons tab
 * 6. remove the blank tab
 *
 * the test succeeds if it doesn't trigger any assertions
 */

add_task(async function test() {
  // Open the test tab
  let testTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:addons");

  // insert button into test page content
  await ContentTask.spawn(gBrowser.selectedBrowser, null, async function() {
    let doc = content.document;
    let e = doc.createElement("button");
    e.setAttribute("label", "hello");
    e.setAttribute("tooltiptext", "world");
    e.setAttribute("id", "test-button");
    doc.documentElement.insertBefore(e, doc.documentElement.firstChild);
  });

  // open a second tab and select it
  let tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", true);
  gBrowser.selectedTab = tab2;

  // Select the testTab then perform mouse events on inserted button
  gBrowser.selectedTab = testTab;
  let browser = gBrowser.selectedBrowser;
  EventUtils.disableNonTestMouseEvents(true);
  try {
    await BrowserTestUtils.synthesizeMouse("#test-button", 1, 1, { type: "mouseover" }, browser);
    await BrowserTestUtils.synthesizeMouse("#test-button", 2, 6, { type: "mousemove" }, browser);
    await BrowserTestUtils.synthesizeMouse("#test-button", 2, 4, { type: "mousemove" }, browser);
  } finally {
    EventUtils.disableNonTestMouseEvents(false);
  }

  // cleanup
  await BrowserTestUtils.removeTab(testTab);
  await BrowserTestUtils.removeTab(tab2);
  ok(true, "pass if no assertions");
});
