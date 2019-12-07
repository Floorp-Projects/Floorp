// First test - open a tab and duplicate it, using session restore to restore the history into the new tab.
add_task(async function duplicateTab() {
  const TEST_URL = "data:text/html,foo";
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await ContentTask.spawn(tab.linkedBrowser, null, function() {
    let docshell = content.window.docShell.QueryInterface(Ci.nsIWebNavigation);
    let shEntry = docshell.sessionHistory.legacySHistory.getEntryAtIndex(0);
    is(shEntry.docshellID.toString(), docshell.historyID.toString());
  });

  let tab2 = gBrowser.duplicateTab(tab);
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);

  await ContentTask.spawn(tab2.linkedBrowser, null, function() {
    let docshell = content.window.docShell.QueryInterface(Ci.nsIWebNavigation);
    let shEntry = docshell.sessionHistory.legacySHistory.getEntryAtIndex(0);
    is(shEntry.docshellID.toString(), docshell.historyID.toString());
  });

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

// Second test - open a tab and navigate across processes, which triggers sessionrestore to persist history.
add_task(async function contentToChromeNavigate() {
  const TEST_URL = "data:text/html,foo";
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await ContentTask.spawn(tab.linkedBrowser, null, function() {
    let docshell = content.window.docShell.QueryInterface(Ci.nsIWebNavigation);
    let sh = docshell.sessionHistory;
    is(sh.count, 1);
    is(
      sh.legacySHistory.getEntryAtIndex(0).docshellID.toString(),
      docshell.historyID.toString()
    );
  });

  // Force the browser to navigate to the chrome process.
  await ContentTask.spawn(tab.linkedBrowser, null, function() {
    const CHROME_URL = "about:config";
    let webnav = content.window.getInterface(Ci.nsIWebNavigation);
    let loadURIOptions = {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    };
    webnav.loadURI(CHROME_URL, loadURIOptions);
  });
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Check to be sure that we're in the chrome process.
  let docShell = tab.linkedBrowser.frameLoader.docShell;

  // 'cause we're in the chrome process, we can just directly poke at the shistory.
  let sh = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;

  is(sh.count, 2);
  is(
    sh.legacySHistory.getEntryAtIndex(0).docshellID.toString(),
    docShell.historyID.toString()
  );
  is(
    sh.legacySHistory.getEntryAtIndex(1).docshellID.toString(),
    docShell.historyID.toString()
  );

  BrowserTestUtils.removeTab(tab);
});
