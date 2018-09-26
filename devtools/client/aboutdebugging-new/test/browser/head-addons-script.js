/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from ../../../shared/test/shared-head.js */

"use strict";

const { Management } = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});

function getSupportsFile(path) {
  const cr = Cc["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Ci.nsIChromeRegistry);
  const uri = Services.io.newURI(CHROME_URL_ROOT + path);
  const fileurl = cr.convertChromeURL(uri);
  return fileurl.QueryInterface(Ci.nsIFileURL);
}

/**
 * Install a temporary extension at the provided path, with the provided name.
 * Will use a mock file picker to select the file.
 */
// eslint-disable-next-line no-unused-vars
async function installTemporaryExtension(path, name, document) {
  // Mock the file picker to select a test addon
  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  const file = getSupportsFile(path);
  MockFilePicker.setFiles([file.file]);

  const onAddonInstalled = new Promise(done => {
    Management.on("startup", function listener(event, extension) {
      if (extension.name != name) {
        return;
      }

      Management.off("startup", listener);
      done();
    });
  });

  // Trigger the file picker by clicking on the button
  document.querySelector(".js-temporary-extension-install-button").click();

  info("Wait for addon to be installed");
  await onAddonInstalled;
}
