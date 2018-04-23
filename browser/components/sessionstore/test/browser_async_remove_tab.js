"use strict";

async function createTabWithRandomValue(url) {
  let tab = BrowserTestUtils.addTab(gBrowser, url);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Set a random value.
  let r = `rand-${Math.random()}`;
  ss.setCustomTabValue(tab, "foobar", r);

  // Flush to ensure there are no scheduled messages.
  await TabStateFlusher.flush(browser);

  return {tab, r};
}

function isValueInClosedData(rval) {
  return ss.getClosedTabData(window).includes(rval);
}

function restoreClosedTabWithValue(rval) {
  let closedTabData = JSON.parse(ss.getClosedTabData(window));
  let index = closedTabData.findIndex(function(data) {
    return (data.state.extData && data.state.extData.foobar) == rval;
  });

  if (index == -1) {
    throw new Error("no closed tab found for given rval");
  }

  return ss.undoCloseTab(window, index);
}

function promiseNewLocationAndHistoryEntryReplaced(browser, snippet) {
  return ContentTask.spawn(browser, snippet, async function(codeSnippet) {
    let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    let shistory = webNavigation.sessionHistory.legacySHistory;

    // Evaluate the snippet that the changes the location.
    // eslint-disable-next-line no-eval
    eval(codeSnippet);

    return new Promise(resolve => {
      let listener = {
        OnHistoryReplaceEntry() {
          shistory.removeSHistoryListener(this);
          resolve();
        },

        QueryInterface: ChromeUtils.generateQI([
          Ci.nsISHistoryListener,
          Ci.nsISupportsWeakReference
        ])
      };

      shistory.addSHistoryListener(listener);

      /* Keep the weak shistory listener alive. */
      addEventListener("unload", function() {
        try {
          shistory.removeSHistoryListener(listener);
        } catch (e) { /* Will most likely fail. */ }
      });
    });
  });
}

function promiseHistoryEntryReplacedNonRemote(browser) {
  let {listeners} = promiseHistoryEntryReplacedNonRemote;

  return new Promise(resolve => {
    let shistory = browser.webNavigation.sessionHistory.legacySHistory;

    let listener = {
      OnHistoryReplaceEntry() {
        shistory.removeSHistoryListener(this);
        executeSoon(resolve);
      },

      QueryInterface: ChromeUtils.generateQI([
        Ci.nsISHistoryListener,
        Ci.nsISupportsWeakReference
      ])
    };

    shistory.addSHistoryListener(listener);
    listeners.set(browser, listener);
  });
}
promiseHistoryEntryReplacedNonRemote.listeners = new WeakMap();

add_task(async function dont_save_empty_tabs() {
  let {tab, r} = await createTabWithRandomValue("about:blank");

  // Remove the tab before the update arrives.
  let promise = promiseRemoveTabAndSessionState(tab);

  // No tab state worth saving.
  ok(!isValueInClosedData(r), "closed tab not saved");
  await promise;

  // Still no tab state worth saving.
  ok(!isValueInClosedData(r), "closed tab not saved");
});

add_task(async function save_worthy_tabs_remote() {
  let {tab, r} = await createTabWithRandomValue("https://example.com/");
  ok(tab.linkedBrowser.isRemoteBrowser, "browser is remote");

  // Remove the tab before the update arrives.
  let promise = promiseRemoveTabAndSessionState(tab);

  // Tab state deemed worth saving.
  ok(isValueInClosedData(r), "closed tab saved");
  await promise;

  // Tab state still deemed worth saving.
  ok(isValueInClosedData(r), "closed tab saved");
});

add_task(async function save_worthy_tabs_nonremote() {
  let {tab, r} = await createTabWithRandomValue("about:robots");
  ok(!tab.linkedBrowser.isRemoteBrowser, "browser is not remote");

  // Remove the tab before the update arrives.
  let promise = promiseRemoveTabAndSessionState(tab);

  // Tab state deemed worth saving.
  ok(isValueInClosedData(r), "closed tab saved");
  await promise;

  // Tab state still deemed worth saving.
  ok(isValueInClosedData(r), "closed tab saved");
});

add_task(async function save_worthy_tabs_remote_final() {
  let {tab, r} = await createTabWithRandomValue("about:blank");
  let browser = tab.linkedBrowser;
  ok(browser.isRemoteBrowser, "browser is remote");

  // Replace about:blank with a new remote page.
  let snippet = 'webNavigation.loadURI("https://example.com/",\
                                       null, null, null, null,\
                                       Services.scriptSecurityManager.getSystemPrincipal())';
  await promiseNewLocationAndHistoryEntryReplaced(browser, snippet);

  // Remotness shouldn't have changed.
  ok(browser.isRemoteBrowser, "browser is still remote");

  // Remove the tab before the update arrives.
  let promise = promiseRemoveTabAndSessionState(tab);

  // No tab state worth saving (that we know about yet).
  ok(!isValueInClosedData(r), "closed tab not saved");
  await promise;

  // Turns out there is a tab state worth saving.
  ok(isValueInClosedData(r), "closed tab saved");
});

add_task(async function save_worthy_tabs_nonremote_final() {
  let {tab, r} = await createTabWithRandomValue("about:blank");
  let browser = tab.linkedBrowser;
  ok(browser.isRemoteBrowser, "browser is remote");

  // Replace about:blank with a non-remote entry.
  await BrowserTestUtils.loadURI(browser, "about:robots");
  ok(!browser.isRemoteBrowser, "browser is not remote anymore");

  // Wait until the new entry replaces about:blank.
  await promiseHistoryEntryReplacedNonRemote(browser);

  // Remove the tab before the update arrives.
  let promise = promiseRemoveTabAndSessionState(tab);

  // No tab state worth saving (that we know about yet).
  ok(!isValueInClosedData(r), "closed tab not saved");
  await promise;

  // Turns out there is a tab state worth saving.
  ok(isValueInClosedData(r), "closed tab saved");
});

add_task(async function dont_save_empty_tabs_final() {
  let {tab, r} = await createTabWithRandomValue("https://example.com/");
  let browser = tab.linkedBrowser;

  // Replace the current page with an about:blank entry.
  let snippet = 'content.location.replace("about:blank")';
  await promiseNewLocationAndHistoryEntryReplaced(browser, snippet);

  // Remove the tab before the update arrives.
  let promise = promiseRemoveTabAndSessionState(tab);

  // Tab state deemed worth saving (yet).
  ok(isValueInClosedData(r), "closed tab saved");
  await promise;

  // Turns out we don't want to save the tab state.
  ok(!isValueInClosedData(r), "closed tab not saved");
});

add_task(async function undo_worthy_tabs() {
  let {tab, r} = await createTabWithRandomValue("https://example.com/");
  ok(tab.linkedBrowser.isRemoteBrowser, "browser is remote");

  // Remove the tab before the update arrives.
  let promise = promiseRemoveTabAndSessionState(tab);

  // Tab state deemed worth saving.
  ok(isValueInClosedData(r), "closed tab saved");

  // Restore the closed tab before receiving its final message.
  tab = restoreClosedTabWithValue(r);

  // Wait for the final update message.
  await promise;

  // Check we didn't add the tab back to the closed list.
  ok(!isValueInClosedData(r), "tab no longer closed");

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
});

add_task(async function forget_worthy_tabs_remote() {
  let {tab, r} = await createTabWithRandomValue("https://example.com/");
  ok(tab.linkedBrowser.isRemoteBrowser, "browser is remote");

  // Remove the tab before the update arrives.
  let promise = promiseRemoveTabAndSessionState(tab);

  // Tab state deemed worth saving.
  ok(isValueInClosedData(r), "closed tab saved");

  // Forget the closed tab.
  ss.forgetClosedTab(window, 0);

  // Wait for the final update message.
  await promise;

  // Check we didn't add the tab back to the closed list.
  ok(!isValueInClosedData(r), "we forgot about the tab");
});
