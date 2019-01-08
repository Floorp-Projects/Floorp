/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

const { Management } = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});

function _getSupportsFile(path) {
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
async function installTemporaryExtension(path, name, document) {
  // Mock the file picker to select a test addon
  prepareMockFilePicker(path);

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
/* exported installTemporaryExtension */

async function removeTemporaryExtension(name, document) {
  info(`Remove the temporary extension with name: '${name}'`);
  const temporaryExtensionItem = findDebugTargetByText(name, document);
  temporaryExtensionItem.querySelector(".js-temporary-extension-remove-button").click();

  info("Wait until the debug target item disappears");
  await waitUntil(() => !findDebugTargetByText(name, document));
}
/* exported removeTemporaryExtension */

function prepareMockFilePicker(path) {
  // Mock the file picker to select a test addon
  const MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  MockFilePicker.setFiles([_getSupportsFile(path).file]);
}
/* exported prepareMockFilePicker */
