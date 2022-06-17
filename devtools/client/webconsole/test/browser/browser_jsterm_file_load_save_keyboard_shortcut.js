/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the keyboard shortcut for loading/saving from the console input work as expected.

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Test load/save keyboard shortcut";
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const LOCAL_FILE_NAME = "snippet.js";
const LOCAL_FILE_ORIGINAL_CONTENT = `"Hello from local file"`;
const LOCAL_FILE_NEW_CONTENT = `"Hello from console input"`;

add_task(async function() {
  info("Open the console");
  const hud = await openNewTabAndConsole(TEST_URI);
  is(getInputValue(hud), "", "Input is empty after opening");

  // create file to import first
  info("Create the file to import");
  const { MockFilePicker } = SpecialPowers;
  MockFilePicker.init(window);
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  const file = await createLocalFile();
  MockFilePicker.setFiles([file]);

  const onFilePickerShown = new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      resolve(fp);
    };
  });

  const isMacOS = Services.appinfo.OS === "Darwin";
  EventUtils.synthesizeKey("O", {
    [isMacOS ? "metaKey" : "ctrlKey"]: true,
  });

  info("Wait for File Picker");
  await onFilePickerShown;

  await waitFor(() => getInputValue(hud) === LOCAL_FILE_ORIGINAL_CONTENT);
  ok(true, "File was imported into console input");

  info("Change the input content");
  await setInputValue(hud, LOCAL_FILE_NEW_CONTENT);

  const nsiFile = FileUtils.getFile("TmpD", [`console_input_${Date.now()}.js`]);
  MockFilePicker.setFiles([nsiFile]);

  info("Save the input content");
  EventUtils.synthesizeKey("S", {
    [isMacOS ? "metaKey" : "ctrlKey"]: true,
  });

  await waitFor(() => IOUtils.exists(nsiFile.path));
  const buffer = await IOUtils.read(nsiFile.path);
  const fileContent = new TextDecoder().decode(buffer);
  is(
    fileContent,
    LOCAL_FILE_NEW_CONTENT,
    "Saved file has the expected content"
  );
  MockFilePicker.reset();
});

async function createLocalFile() {
  const file = FileUtils.getFile("TmpD", [LOCAL_FILE_NAME]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  await writeInFile(LOCAL_FILE_ORIGINAL_CONTENT, file);
  return file;
}

function writeInFile(string, file) {
  const inputStream = getInputStream(string);
  const outputStream = FileUtils.openSafeFileOutputStream(file);

  return new Promise((resolve, reject) => {
    NetUtil.asyncCopy(inputStream, outputStream, status => {
      if (!Components.isSuccessCode(status)) {
        reject(new Error("Could not save data to file."));
      }
      resolve();
    });
  });
}
