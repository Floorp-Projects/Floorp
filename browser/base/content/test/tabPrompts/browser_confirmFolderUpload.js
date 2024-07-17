/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

/**
 * Create a temporary test directory that will be cleaned up on test shutdown.
 * @returns {String} - absolute directory path.
 */
function getTestDirectory() {
  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tmpDir.append("testdir");
  if (!tmpDir.exists()) {
    tmpDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    registerCleanupFunction(() => {
      tmpDir.remove(true);
    });
  }

  let file1 = tmpDir.clone();
  file1.append("foo.txt");
  if (!file1.exists()) {
    file1.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
  }

  let file2 = tmpDir.clone();
  file2.append("bar.txt");
  if (!file2.exists()) {
    file2.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
  }

  return tmpDir.path;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Allow using our MockFilePicker in the content process.
      ["dom.filesystem.pathcheck.disabled", true],
      ["dom.webkitBlink.dirPicker.enabled", true],
    ],
  });
});

/**
 * Create a file input, select a folder and wait for the upload confirmation
 * prompt to open.
 * @param {boolean} confirmUpload - Whether to accept (true) or cancel the
 * prompt (false).
 * @returns {Promise} - Resolves once the prompt has been closed.
 */
async function testUploadPrompt(confirmUpload) {
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    // Create file input element
    await ContentTask.spawn(browser, null, () => {
      let input = content.document.createElement("input");
      input.id = "filepicker";
      input.setAttribute("type", "file");
      input.setAttribute("webkitdirectory", "");
      content.document.body.appendChild(input);
    });

    // If we're confirming the dialog, register a  "change" listener on the
    // file input.
    let changePromise;
    if (confirmUpload) {
      changePromise = ContentTask.spawn(browser, null, async () => {
        let input = content.document.getElementById("filepicker");
        return ContentTaskUtils.waitForEvent(input, "change").then(
          e => e.target.files.length
        );
      });
    }

    // Register prompt promise
    let promptPromise = PromptTestUtils.waitForPrompt(browser, {
      modalType: Services.prompt.MODAL_TYPE_TAB,
      promptType: "confirmEx",
    });

    // Open filepicker
    let path = getTestDirectory();
    await ContentTask.spawn(browser, { path }, args => {
      let MockFilePicker = content.SpecialPowers.MockFilePicker;
      MockFilePicker.init(
        content.browsingContext,
        "A Mock File Picker",
        content.SpecialPowers.Ci.nsIFilePicker.modeGetFolder
      );
      MockFilePicker.useDirectory(args.path);

      let input = content.document.getElementById("filepicker");

      // Activate the page to allow opening the file picker.
      content.SpecialPowers.wrap(
        content.document
      ).notifyUserGestureActivation();

      input.click();
    });

    // Wait for confirmation prompt
    let prompt = await promptPromise;
    ok(prompt, "Shown upload confirmation prompt");
    is(prompt.ui.button0.label, "Upload", "Accept button label");
    ok(prompt.ui.button1.hasAttribute("default"), "Cancel is default button");

    // Close confirmation prompt
    await PromptTestUtils.handlePrompt(prompt, {
      buttonNumClick: confirmUpload ? 0 : 1,
    });

    // If we accepted, wait for the input elements "change" event
    if (changePromise) {
      let fileCount = await changePromise;
      is(fileCount, 2, "Should have selected 2 files");
    } else {
      let fileCount = await ContentTask.spawn(browser, null, () => {
        return content.document.getElementById("filepicker").files.length;
      });

      is(fileCount, 0, "Should not have selected any files");
    }

    // Cleanup
    await ContentTask.spawn(browser, null, () => {
      content.SpecialPowers.MockFilePicker.cleanup();
    });
  });
}

// Tests the confirmation prompt that shows after the user picked a folder.

// Confirm the prompt
add_task(async function test_confirm() {
  await testUploadPrompt(true);
});

// Cancel the prompt
add_task(async function test_cancel() {
  await testUploadPrompt(false);
});
