/**
 * Provides infrastructure for automated formautofill components tests.
 */

"use strict";

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);
var { FormLikeFactory } = ChromeUtils.import(
  "resource://gre/modules/FormLikeFactory.jsm"
);
var { AddonTestUtils, MockAsyncShutdown } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
var { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);
var { FileTestUtils } = ChromeUtils.import(
  "resource://testing-common/FileTestUtils.jsm"
);
var { MockDocument } = ChromeUtils.import(
  "resource://testing-common/MockDocument.jsm"
);
var { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
var { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AddonManagerPrivate",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadPaths",
  "resource://gre/modules/DownloadPaths.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

{
  // We're going to register a mock file source
  // with region names based on en-US. This is
  // necessary for tests that expect to match
  // on region code display names.
  const fs = [
    {
      path: "toolkit/intl/regionNames.ftl",
      source: `
region-name-us = United States
region-name-nz = New Zealand
region-name-au = Australia
region-name-ca = Canada
region-name-tw = Taiwan
    `,
    },
  ];

  let locales = Services.locale.packagedLocales;
  const mockSource = L10nFileSource.createMock(
    "mock",
    "app",
    locales,
    "resource://mock_path",
    fs
  );
  L10nRegistry.getInstance().registerSources([mockSource]);
}

do_get_profile();

const EXTENSION_ID = "formautofill@mozilla.org";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

function SetPref(name, value) {
  switch (typeof value) {
    case "string":
      Services.prefs.setCharPref(name, value);
      break;
    case "number":
      Services.prefs.setIntPref(name, value);
      break;
    case "boolean":
      Services.prefs.setBoolPref(name, value);
      break;
    default:
      throw new Error("Unknown type");
  }
}

async function loadExtension() {
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "1.9.2"
  );
  await AddonTestUtils.promiseStartupManager();

  let extensionPath = Services.dirsvc.get("GreD", Ci.nsIFile);
  extensionPath.append("browser");
  extensionPath.append("features");
  extensionPath.append(EXTENSION_ID);

  if (!extensionPath.exists()) {
    extensionPath.leafName = `${EXTENSION_ID}.xpi`;
  }

  let startupPromise = new Promise(resolve => {
    const { apiManager } = ExtensionParent;
    function onReady(event, extension) {
      if (extension.id == EXTENSION_ID) {
        apiManager.off("ready", onReady);
        resolve();
      }
    }

    apiManager.on("ready", onReady);
  });

  await AddonManager.installTemporaryAddon(extensionPath);
  await startupPromise;
}

// Returns a reference to a temporary file that is guaranteed not to exist and
// is cleaned up later. See FileTestUtils.getTempFile for details.
function getTempFile(leafName) {
  return FileTestUtils.getTempFile(leafName);
}

async function initProfileStorage(
  fileName,
  records,
  collectionName = "addresses"
) {
  let { FormAutofillStorage } = ChromeUtils.import(
    "resource://autofill/FormAutofillStorage.jsm"
  );
  let path = getTempFile(fileName).path;
  let profileStorage = new FormAutofillStorage(path);
  await profileStorage.initialize();

  // AddonTestUtils inserts its own directory provider that manages TmpD.
  // It removes that directory at shutdown, which races with shutdown
  // handing in JSONFile/DeferredTask (which is used by FormAutofillStorage).
  // Avoid the race by explicitly finalizing any formautofill JSONFile
  // instances created manually by individual tests when the test finishes.
  registerCleanupFunction(function finalizeAutofillStorage() {
    return profileStorage._finalize();
  });

  if (!records || !Array.isArray(records)) {
    return profileStorage;
  }

  let onChanged = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) =>
      data == "add" && subject.wrappedJSObject.collectionName == collectionName
  );
  for (let record of records) {
    Assert.ok(await profileStorage[collectionName].add(record));
    await onChanged;
  }
  await profileStorage._saveImmediately();
  return profileStorage;
}

function verifySectionFieldDetails(sections, expectedResults) {
  Assert.equal(
    sections.length,
    expectedResults.length,
    "Expected section count."
  );
  sections.forEach((sectionInfo, sectionIndex) => {
    let expectedSectionInfo = expectedResults[sectionIndex];
    info("FieldName Prediction Results: " + sectionInfo.map(i => i.fieldName));
    info(
      "FieldName Expected Results:   " +
        expectedSectionInfo.map(i => i.fieldName)
    );
    Assert.equal(
      sectionInfo.length,
      expectedSectionInfo.length,
      "Expected field count."
    );

    sectionInfo.forEach((field, fieldIndex) => {
      let expectedField = expectedSectionInfo[fieldIndex];
      delete field._reason;
      delete field.elementWeakRef;
      delete field.confidence;
      Assert.deepEqual(field, expectedField);
    });
  });
}

var FormAutofillHeuristics, LabelUtils;
var AddressDataLoader, FormAutofillUtils;

async function runHeuristicsTest(patterns, fixturePathPrefix) {
  add_setup(async () => {
    ({ FormAutofillHeuristics } = ChromeUtils.import(
      "resource://autofill/FormAutofillHeuristics.jsm"
    ));
    ({ AddressDataLoader, FormAutofillUtils, LabelUtils } = ChromeUtils.import(
      "resource://autofill/FormAutofillUtils.jsm"
    ));
  });

  patterns.forEach(testPattern => {
    add_task(async function() {
      info("Starting test fixture: " + testPattern.fixturePath);
      let file = do_get_file(fixturePathPrefix + testPattern.fixturePath);
      let doc = MockDocument.createTestDocumentFromFile(
        "http://localhost:8080/test/",
        file
      );

      let forms = [];

      for (let field of FormAutofillUtils.autofillFieldSelector(doc)) {
        let formLike = FormLikeFactory.createFromField(field);
        if (!forms.some(form => form.rootElement === formLike.rootElement)) {
          forms.push(formLike);
        }
      }

      Assert.equal(
        forms.length,
        testPattern.expectedResult.length,
        "Expected form count."
      );

      forms.forEach((form, formIndex) => {
        let sections = FormAutofillHeuristics.getFormInfo(form);
        verifySectionFieldDetails(
          sections.map(section => section.fieldDetails),
          testPattern.expectedResult[formIndex]
        );
      });
    });
  });
}

/**
 * Returns the Sync change counter for a profile storage record. Synced records
 * store additional metadata for tracking changes and resolving merge conflicts.
 * Deleting a synced record replaces the record with a tombstone.
 *
 * @param   {AutofillRecords} records
 *          The `AutofillRecords` instance to query.
 * @param   {string} guid
 *          The GUID of the record or tombstone.
 * @returns {number}
 *          The change counter, or -1 if the record doesn't exist or hasn't
 *          been synced yet.
 */
function getSyncChangeCounter(records, guid) {
  let record = records._findByGUID(guid, { includeDeleted: true });
  if (!record) {
    return -1;
  }
  let sync = records._getSyncMetaData(record);
  if (!sync) {
    return -1;
  }
  return sync.changeCounter;
}

/**
 * Performs a partial deep equality check to determine if an object contains
 * the given fields.
 *
 * @param   {Object} object
 *          The object to check. Unlike `ObjectUtils.deepEqual`, properties in
 *          `object` that are not in `fields` will be ignored.
 * @param   {Object} fields
 *          The fields to match.
 * @returns {boolean}
 *          Does `object` contain `fields` with matching values?
 */
function objectMatches(object, fields) {
  let actual = {};
  for (let key in fields) {
    if (!object.hasOwnProperty(key)) {
      return false;
    }
    actual[key] = object[key];
  }
  return ObjectUtils.deepEqual(actual, fields);
}

add_setup(async function head_initialize() {
  Services.prefs.setBoolPref("extensions.experiments.enabled", true);
  Services.prefs.setBoolPref(
    "extensions.formautofill.heuristics.enabled",
    true
  );
  Services.prefs.setBoolPref("extensions.formautofill.section.enabled", true);
  Services.prefs.setBoolPref("dom.forms.autocomplete.formautofill", true);

  Services.prefs.setCharPref(
    "extensions.formautofill.addresses.supported",
    "on"
  );
  Services.prefs.setCharPref(
    "extensions.formautofill.creditCards.supported",
    "on"
  );
  Services.prefs.setBoolPref("extensions.formautofill.addresses.enabled", true);
  Services.prefs.setBoolPref(
    "extensions.formautofill.creditCards.enabled",
    true
  );

  // Clean up after every test.
  registerCleanupFunction(function head_cleanup() {
    Services.prefs.clearUserPref("extensions.experiments.enabled");
    Services.prefs.clearUserPref(
      "extensions.formautofill.creditCards.supported"
    );
    Services.prefs.clearUserPref("extensions.formautofill.addresses.supported");
    Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");
    Services.prefs.clearUserPref("extensions.formautofill.heuristics.enabled");
    Services.prefs.clearUserPref("extensions.formautofill.section.enabled");
    Services.prefs.clearUserPref("dom.forms.autocomplete.formautofill");
    Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");
    Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");
  });

  await loadExtension();
});

let OSKeyStoreTestUtils;
add_setup(async function os_key_store_setup() {
  ({ OSKeyStoreTestUtils } = ChromeUtils.import(
    "resource://testing-common/OSKeyStoreTestUtils.jsm"
  ));
  OSKeyStoreTestUtils.setup();
  registerCleanupFunction(async function cleanup() {
    await OSKeyStoreTestUtils.cleanup();
  });
});
