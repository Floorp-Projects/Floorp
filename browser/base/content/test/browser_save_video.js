/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * TestCase for bug 564387
 * <https://bugzilla.mozilla.org/show_bug.cgi?id=564387>
 */
function test() {

  // --- Testing support library ---

  // Import the toolkit test support library in the scope of the current test.
  // This operation also defines the common constants Cc, Ci, Cu, Cr and Cm.
  Components.classes["@mozilla.org/moz/jssubscript-loader;1"].
   getService(Components.interfaces.mozIJSSubScriptLoader).loadSubScript(
   "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/_loadAll.js",
   this);

  // --- Test implementation ---

  const kBaseUrl =
        "http://mochi.test:8888/browser/browser/base/content/test/";

  function pageShown(event) {
    if (event.target.location != "about:blank")
      testRunner.continueTest();
  }

  function saveVideoAs_TestGenerator() {
    // Load Test page
    gBrowser.addEventListener("pageshow", pageShown, false);
    gBrowser.loadURI(kBaseUrl + "bug564387.html");
    yield;
    gBrowser.removeEventListener("pageshow", pageShown, false);

    // Ensure that the window is focused.
    SimpleTest.waitForFocus(testRunner.continueTest);
    yield;

    try {
      // get the video element
      var video1 = gBrowser.contentDocument.getElementById("video1");

      // Synthesize the right click on the context menu, and
      // wait for it to be shown
      document.addEventListener("popupshown", testRunner.continueTest, false);
      EventUtils.synthesizeMouseAtCenter(video1,
                                         { type: "contextmenu", button: 2 },
                                         gBrowser.contentWindow);
      yield;

      // Create the folder the video will be saved into.
      var destDir = createTemporarySaveDirectory();
      try {
        // Call the appropriate save function defined in contentAreaUtils.js.
        mockFilePickerSettings.destDir = destDir;
        mockFilePickerSettings.filterIndex = 1; // kSaveAsType_URL

        // register mock file picker object
        mockFilePickerRegisterer.register();
        try {
          // register mock download progress listener
          mockTransferForContinuingRegisterer.register();
          try {
            // Select "Save Video As" option from context menu
            var saveVideoCommand = document.getElementById("context-savevideo");
            saveVideoCommand.doCommand();

            // Unregister the popupshown listener
            document.removeEventListener("popupshown",
                                         testRunner.continueTest, false);
            // Close the context menu
            document.getElementById("placesContext").hidePopup();

            // Wait for the download to finish, and exit if it wasn't successful.
            var downloadSuccess = yield;
            if (!downloadSuccess)
              throw "Unexpected failure in downloading Video file!";
          }
          finally {
            // unregister download progress listener
            mockTransferForContinuingRegisterer.unregister();
          }
        }
        finally {
          // unregister mock file picker object
          mockFilePickerRegisterer.unregister();
        }

        // Read the name of the saved file.
        var fileName = mockFilePickerResults.selectedFile.leafName;

        is(fileName, "Bug564387-expectedName.ogv",
                     "Video File Name is correctly retrieved from Content-Disposition http header");
      }
      finally {
        // Clean up the saved file.
        destDir.remove(true);
      }
    }
    finally {
      // Replace the current tab with a clean one.
      gBrowser.addTab().linkedBrowser.stop();
      gBrowser.removeCurrentTab();
    }
  }

  // --- Run the test ---
  testRunner.runTest(saveVideoAs_TestGenerator);
}
