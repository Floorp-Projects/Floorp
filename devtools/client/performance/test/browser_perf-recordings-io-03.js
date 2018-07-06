/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";
/* eslint-disable */
/**
 * Tests if the performance tool gracefully handles loading files that are JSON,
 * but don't contain the appropriate recording data.
 */

var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm", {});
var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm", {});

var test = async function () {
  let { target, panel, toolbox } = await initPerformance(SIMPLE_URL);
  let { EVENTS, PerformanceController } = panel.panelWin;

  let file = FileUtils.getFile("TmpD", ["tmpprofile.json"]);
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));
  await asyncCopy({ bogus: "data" }, file);

  try {
    await PerformanceController.importRecording("", file);
    ok(false, "The recording succeeded unexpectedly.");
  } catch (e) {
    is(e.message, "Unrecognized recording data file.", "Message is correct.");
    ok(true, "The recording was cancelled.");
  }

  await teardown(panel);
  finish();
};

function getUnicodeConverter() {
  let className = "@mozilla.org/intl/scriptableunicodeconverter";
  let converter = Cc[className].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter;
}

function asyncCopy(data, file) {
  let string = JSON.stringify(data);
  let inputStream = getUnicodeConverter().convertToInputStream(string);
  let outputStream = FileUtils.openSafeFileOutputStream(file);

  return new Promise((resolve, reject) => {
    NetUtil.asyncCopy(inputStream, outputStream, status => {
      if (!Components.isSuccessCode(status)) {
        reject(new Error("Could not save data to file."));
      }
      resolve();
    });
  });
}
/* eslint-enable */
