/**
 * Provides infrastructure for automated login components tests.
 */

 /* exported importAutofillModule, getTempFile */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/MockDocument.jsm");

// Load the module by Service newFileURI API for running extension's XPCShell test
function importAutofillModule(module) {
  return Cu.import(Services.io.newFileURI(do_get_file(module)).spec);
}

XPCOMUtils.defineLazyModuleGetter(this, "DownloadPaths",
                                  "resource://gre/modules/DownloadPaths.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

// While the previous test file should have deleted all the temporary files it
// used, on Windows these might still be pending deletion on the physical file
// system.  Thus, start from a new base number every time, to make a collision
// with a file that is still pending deletion highly unlikely.
let gFileCounter = Math.floor(Math.random() * 1000000);

/**
 * Returns a reference to a temporary file, that is guaranteed not to exist, and
 * to have never been created before.
 *
 * @param {string} leafName
 *        Suggested leaf name for the file to be created.
 *
 * @returns {nsIFile} pointing to a non-existent file in a temporary directory.
 *
 * @note It is not enough to delete the file if it exists, or to delete the file
 *       after calling nsIFile.createUnique, because on Windows the delete
 *       operation in the file system may still be pending, preventing a new
 *       file with the same name to be created.
 */
function getTempFile(leafName) {
  // Prepend a serial number to the extension in the suggested leaf name.
  let [base, ext] = DownloadPaths.splitBaseNameAndExtension(leafName);
  let finalLeafName = base + "-" + gFileCounter + ext;
  gFileCounter++;

  // Get a file reference under the temporary directory for this test file.
  let file = FileUtils.getFile("TmpD", [finalLeafName]);
  do_check_false(file.exists());

  do_register_cleanup(function() {
    if (file.exists()) {
      file.remove(false);
    }
  });

  return file;
}

add_task(function* test_common_initialize() {
  Services.prefs.setBoolPref("dom.forms.autocomplete.experimental", true);

  // Clean up after every test.
  do_register_cleanup(() => {
    Services.prefs.setBoolPref("dom.forms.autocomplete.experimental", false);
  });
});
