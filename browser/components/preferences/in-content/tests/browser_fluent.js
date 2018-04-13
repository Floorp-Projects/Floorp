
function whenMainPaneLoadedFinished() {
  return new Promise(function(resolve, reject) {
    const topic = "main-pane-loaded";
    Services.obs.addObserver(function observer(aSubject) {
      Services.obs.removeObserver(observer, topic);
      resolve();
    }, topic);
  });
}

// Temporary test for an experimental new localization API.
// See bug 1402069 for details.
add_task(async function() {
  // The string is used only when `browserTabsRemoteAutostart` is true
  if (!Services.appinfo.browserTabsRemoteAutostart) {
    ok(true, "fake test to avoid harness complaining");
    return;
  }

  await Promise.all([
    openPreferencesViaOpenPreferencesAPI("general", {leaveOpen: true}),
    whenMainPaneLoadedFinished(),
  ]);

  let doc = gBrowser.contentDocument;
  let win = gBrowser.contentWindow;
  await doc.l10n.ready;

  let processCountPref = win.Preferences.get("dom.ipc.processCount");
  let defaultProcessCount = processCountPref.defaultValue;

  let [ msg ] = await doc.l10n.formatMessages([
    ["performance-default-content-process-count", { num: defaultProcessCount }]
  ]);

  let elem = doc.querySelector(
    `#contentProcessCount > menupopup > menuitem[value="${defaultProcessCount}"]`);

  Assert.deepEqual(msg, {
    value: null,
    attributes: [
      {name: "label", value: elem.getAttribute("label")}
    ]
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
