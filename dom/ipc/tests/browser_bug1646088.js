let dir = getChromeDir(getResolvedURI(gTestPath));
dir.append("file_dummy.html");
const uriString = Services.io.newFileURI(dir).spec;

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    "https://example.com",
    async function (browser) {
      // Override the browser's `prepareToChangeRemoteness` so that we can delay
      // the process switch for an indefinite amount of time. This will allow us
      // to control the timing of the resolve call to trigger the bug.
      let prepareToChangeCalled = PromiseUtils.defer();
      let finishSwitch = PromiseUtils.defer();
      let oldPrepare = browser.prepareToChangeRemoteness;
      browser.prepareToChangeRemoteness = async () => {
        prepareToChangeCalled.resolve();
        await oldPrepare.call(browser);
        await finishSwitch.promise;
      };

      // Begin a process switch, which should cause `prepareToChangeRemoteness` to
      // be called.
      // NOTE: This used to avoid BrowserTestUtils.loadURI, as that call would
      // previously eagerly perform a process switch meaning that the interesting
      // codepath wouldn't be triggered. Nowadays the process switch codepath
      // always happens during navigation as required by this test.
      info("Beginning process switch into file URI process");
      let browserLoaded = BrowserTestUtils.browserLoaded(browser);
      BrowserTestUtils.startLoadingURIString(browser, uriString);
      await prepareToChangeCalled.promise;

      // The tab we opened is now midway through process switching. Open another
      // browser within the same tab, and immediately close it after the load
      // finishes.
      info("Creating new tab loaded in file URI process");
      let fileProcess;
      let browserParentDestroyed = PromiseUtils.defer();
      await BrowserTestUtils.withNewTab(
        uriString,
        async function (otherBrowser) {
          let remoteTab = otherBrowser.frameLoader.remoteTab;
          fileProcess = remoteTab.contentProcessId;
          info("Loaded test URI in pid: " + fileProcess);

          browserParentDestroyed.resolve(
            TestUtils.topicObserved(
              "ipc:browser-destroyed",
              subject => subject === remoteTab
            )
          );
        }
      );
      await browserParentDestroyed.promise;

      // This browser has now been closed, which could cause the file content
      // process to begin shutting down, despite us process switching into it.
      // We can now allow the process switch to finish, and wait for the load to
      // finish as well.
      info("BrowserParent has been destroyed, finishing process switch");
      finishSwitch.resolve();
      await browserLoaded;

      info("Load complete");
      is(
        browser.frameLoader.remoteTab.contentProcessId,
        fileProcess,
        "Should have loaded in the same file URI process"
      );
    }
  );
});
