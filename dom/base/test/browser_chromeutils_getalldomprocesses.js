add_task(async function testParentProcess() {
  const [parentProcess] = ChromeUtils.getAllDOMProcesses();
  // browser.xhtml is in the parent process, so its domProcess is the parent process one
  is(
    parentProcess,
    window.browsingContext.currentWindowGlobal.domProcess,
    "The first element is the parent process"
  );
  is(
    parentProcess.osPid,
    Services.appinfo.processID,
    "We got the right OS Pid"
  );
  is(parentProcess.childID, 0, "Parent process has childID set to 0");
});

add_task(async function testContentProcesses() {
  info("Open a tab in a content process");
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "about:blank"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  info("Sanity checks against all returned elements of getAllDOMProcesses");
  const processes = ChromeUtils.getAllDOMProcesses();
  const allPids = [],
    allChildIDs = [];
  for (const process of processes) {
    ok(
      process instanceof Ci.nsIDOMProcessParent,
      `Each element of the array is a nsIDOMProcessParent (${process})`
    );
    ok(process.osPid > 0, `OS Pid looks correct ${process.osPid}`);
    if (process == processes[0]) {
      is(
        process.childID,
        0,
        `Child ID is 0 for the parent process, which is the first element of the returned array`
      );
      is(
        process.remoteType,
        null,
        "The main process's remote type should be NOT_REMOTE"
      );
    } else {
      ok(process.childID > 0, `Child ID looks also correct ${process.childID}`);
      ok(process.remoteType, "Should have a remote type");
    }

    ok(
      !allPids.includes(process.osPid),
      "We only get one nsIDOMProcessParent per OS process == each osPid is different"
    );
    allPids.push(process.osPid);
    ok(!allChildIDs.includes(process.childID), "Each childID is different");
    allChildIDs.push(process.childID);
  }

  info("Search for the nsIDOMProcessParent for the opened tab");
  const { currentWindowGlobal } = gBrowser.selectedBrowser.browsingContext;
  const tabProcess = currentWindowGlobal.domProcess;
  ok(processes.includes(tabProcess), "Found the tab process in the list");
  is(tabProcess.osPid, currentWindowGlobal.osPid, "We got the right OS Pid");
});
