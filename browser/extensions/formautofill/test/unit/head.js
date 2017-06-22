/**
 * Provides infrastructure for automated formautofill components tests.
 */

/* exported getTempFile, loadFormAutofillContent, runHeuristicsTest, sinon,
 *          initProfileStorage
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FormLikeFactory.jsm");
Cu.import("resource://testing-common/MockDocument.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DownloadPaths",
                                  "resource://gre/modules/DownloadPaths.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

do_get_profile();

// ================================================
// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/releases/v2.3.2/
Cu.import("resource://gre/modules/Timer.jsm");
const {Loader} = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
const loader = new Loader.Loader({
  paths: {
    "": "resource://testing-common/",
  },
  globals: {
    setTimeout,
    setInterval,
    clearTimeout,
    clearInterval,
  },
});
const require = Loader.Require(loader, {id: ""});
const sinon = require("sinon-2.3.2");
// ================================================

// Load our bootstrap extension manifest so we can access our chrome/resource URIs.
const EXTENSION_ID = "formautofill@mozilla.org";
let extensionDir = Services.dirsvc.get("GreD", Ci.nsIFile);
extensionDir.append("browser");
extensionDir.append("features");
extensionDir.append(EXTENSION_ID);
// If the unpacked extension doesn't exist, use the packed version.
if (!extensionDir.exists()) {
  extensionDir = extensionDir.parent;
  extensionDir.leafName += ".xpi";
}
Components.manager.addBootstrappedManifestLocation(extensionDir);

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

async function initProfileStorage(fileName, records) {
  let {ProfileStorage} = Cu.import("resource://formautofill/ProfileStorage.jsm", {});
  let path = getTempFile(fileName).path;
  let profileStorage = new ProfileStorage(path);
  await profileStorage.initialize();

  if (!records || !Array.isArray(records)) {
    return profileStorage;
  }

  let onChanged = TestUtils.topicObserved("formautofill-storage-changed",
                                          (subject, data) => data == "add");
  for (let record of records) {
    do_check_true(profileStorage.addresses.add(record));
    await onChanged;
  }
  await profileStorage._saveImmediately();
  return profileStorage;
}

function runHeuristicsTest(patterns, fixturePathPrefix) {
  Cu.import("resource://formautofill/FormAutofillHeuristics.jsm");
  Cu.import("resource://formautofill/FormAutofillUtils.jsm");

  patterns.forEach(testPattern => {
    add_task(async function() {
      do_print("Starting test fixture: " + testPattern.fixturePath);
      let file = do_get_file(fixturePathPrefix + testPattern.fixturePath);
      let doc = MockDocument.createTestDocumentFromFile("http://localhost:8080/test/", file);

      let forms = [];

      for (let field of FormAutofillUtils.autofillFieldSelector(doc)) {
        let formLike = FormLikeFactory.createFromField(field);
        if (!forms.some(form => form.rootElement === formLike.rootElement)) {
          forms.push(formLike);
        }
      }

      Assert.equal(forms.length, testPattern.expectedResult.length, "Expected form count.");

      forms.forEach((form, formIndex) => {
        let formInfo = FormAutofillHeuristics.getFormInfo(form);
        do_print("FieldName Prediction Results: " + formInfo.map(i => i.fieldName));
        Assert.equal(formInfo.length, testPattern.expectedResult[formIndex].length, "Expected field count.");
        formInfo.forEach((field, fieldIndex) => {
          let expectedField = testPattern.expectedResult[formIndex][fieldIndex];
          expectedField.elementWeakRef = field.elementWeakRef;
          Assert.deepEqual(field, expectedField);
        });
      });
    });
  });
}

add_task(async function head_initialize() {
  Services.prefs.setBoolPref("extensions.formautofill.experimental", true);
  Services.prefs.setBoolPref("extensions.formautofill.heuristics.enabled", true);
  Services.prefs.setBoolPref("dom.forms.autocomplete.experimental", true);

  // Clean up after every test.
  do_register_cleanup(function head_cleanup() {
    Services.prefs.clearUserPref("extensions.formautofill.experimental");
    Services.prefs.clearUserPref("extensions.formautofill.heuristics.enabled");
    Services.prefs.clearUserPref("dom.forms.autocomplete.experimental");
  });
});
