// First test - open a tab and duplicate it, using session restore to restore the history into the new tab.
add_task(async function duplicateTab() {
  const TEST_URL = "data:text/html,foo";
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await ContentTask.spawn(tab.linkedBrowser, null, function() {
    let docshell = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell);
    let shEntry = docshell.sessionHistory.getEntryAtIndex(0, false);
    is(shEntry.docshellID.toString(), docshell.historyID.toString());
  });

  let tab2 = gBrowser.duplicateTab(tab);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  await ContentTask.spawn(tab2.linkedBrowser, null, function() {
    let docshell = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell);
    let shEntry = docshell.sessionHistory.getEntryAtIndex(0, false);
    is(shEntry.docshellID.toString(), docshell.historyID.toString());
  });

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.removeTab(tab2);
});

// Second test - open a tab and navigate across processes, which triggers sessionrestore to persist history.
add_task(async function contentToChromeNavigate() {
  const TEST_URL = "data:text/html,foo";
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await ContentTask.spawn(tab.linkedBrowser, null, function() {
    let docshell = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell);
    let sh = docshell.sessionHistory;
    is(sh.count, 1);
    is(sh.getEntryAtIndex(0, false).docshellID.toString(), docshell.historyID.toString());
  });

  // Force the browser to navigate to the chrome process.
  await ContentTask.spawn(tab.linkedBrowser, null, function() {
    const CHROME_URL = "about:config";
    let webnav = content.window.QueryInterface(Ci.nsIInterfaceRequestor)
                               .getInterface(Ci.nsIWebNavigation);
    webnav.loadURI(CHROME_URL, Ci.nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
  });
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Check to be sure that we're in the chrome process.
  let docShell = tab.linkedBrowser.frameLoader.docShell;

  // 'cause we're in the chrome process, we can just directly poke at the shistory.
  let sh = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;

  is(sh.count, 2);
  is(sh.getEntryAtIndex(0, false).docshellID.toString(), docShell.historyID.toString());
  is(sh.getEntryAtIndex(1, false).docshellID.toString(), docShell.historyID.toString());

  await BrowserTestUtils.removeTab(tab);
});
