/**
 * Provides infrastructure for automated formautofill components tests.
 */

"use strict";

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
var { ObjectUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ObjectUtils.sys.mjs"
);
var { FormLikeFactory } = ChromeUtils.importESModule(
  "resource://gre/modules/FormLikeFactory.sys.mjs"
);
var { FormAutofillHandler } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormAutofillHandler.sys.mjs"
);
var { AddonTestUtils, MockAsyncShutdown } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);
var { ExtensionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ExtensionXPCShellUtils.sys.mjs"
);
var { FileTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/FileTestUtils.sys.mjs"
);
var { MockDocument } = ChromeUtils.importESModule(
  "resource://testing-common/MockDocument.sys.mjs"
);
var { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);
var { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.sys.mjs",
  ExtensionParent: "resource://gre/modules/ExtensionParent.sys.mjs",
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
});

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

/**
 * Mock the return value of Services.focus.elementIsFocusable
 * since a field's focusability can't be tested in a unit test.
 */
(function ignoreAFieldsFocusability() {
  let stub = sinon.stub(Services, "focus").get(() => {
    return {
      elementIsFocusable() {
        return true;
      },
    };
  });

  registerCleanupFunction(() => {
    stub.restore();
  });
})();

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

// Return the current date rounded in the manner that sync does.
function getDateForSync() {
  return Math.round(Date.now() / 10) / 100;
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
  let { FormAutofillStorage } = ChromeUtils.importESModule(
    "resource://autofill/FormAutofillStorage.sys.mjs"
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

function verifySectionAutofillResult(sections, expectedSectionsInfo) {
  sections.forEach((section, index) => {
    const expectedSection = expectedSectionsInfo[index];

    const fieldDetails = section.fieldDetails;
    const expectedFieldDetails = expectedSection.fields;

    info(`verify autofill section[${index}]`);

    fieldDetails.forEach((field, fieldIndex) => {
      const expeceted = expectedFieldDetails[fieldIndex];

      Assert.equal(
        expeceted.autofill,
        field.element.value,
        `Autofilled value for element(id=${field.element.id}, field name=${field.fieldName}) should be equal`
      );
    });
  });
}

function verifySectionFieldDetails(sections, expectedSectionsInfo) {
  sections.forEach((section, index) => {
    const expectedSection = expectedSectionsInfo[index];

    const fieldDetails = section.fieldDetails;
    const expectedFieldDetails = expectedSection.fields;

    info(`section[${index}] ${expectedSection.description ?? ""}:`);
    info(`FieldName Prediction Results: ${fieldDetails.map(i => i.fieldName)}`);
    info(
      `FieldName Expected Results:   ${expectedFieldDetails.map(
        detail => detail.fieldName
      )}`
    );
    Assert.equal(
      fieldDetails.length,
      expectedFieldDetails.length,
      `Expected field count.`
    );

    fieldDetails.forEach((field, fieldIndex) => {
      const expectedFieldDetail = expectedFieldDetails[fieldIndex];

      const expected = {
        ...{
          reason: "autocomplete",
          section: "",
          contactType: "",
          addressType: "",
        },
        ...expectedSection.default,
        ...expectedFieldDetail,
      };

      const keys = new Set([...Object.keys(field), ...Object.keys(expected)]);
      ["autofill", "elementWeakRef", "confidence", "part"].forEach(k =>
        keys.delete(k)
      );

      for (const key of keys) {
        const expectedValue = expected[key];
        const actualValue = field[key];
        Assert.equal(
          expectedValue,
          actualValue,
          `${key} should be equal, expect ${expectedValue}, got ${actualValue}`
        );
      }
    });

    Assert.equal(
      section.isValidSection(),
      !expectedSection.invalid,
      `Should be an ${expectedSection.invalid ? "invalid" : "valid"} section`
    );
  });
}

var FormAutofillHeuristics, LabelUtils;
var AddressDataLoader, FormAutofillUtils;

function autofillFieldSelector(doc) {
  return doc.querySelectorAll("input, select");
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
 * the given fields. To ensure the object doesn't contain a property, set the
 * property of the `fields` object to `undefined`
 *
 * @param   {object} object
 *          The object to check. Unlike `ObjectUtils.deepEqual`, properties in
 *          `object` that are not in `fields` will be ignored.
 * @param   {object} fields
 *          The fields to match.
 * @returns {boolean}
 *          Does `object` contain `fields` with matching values?
 */
function objectMatches(object, fields) {
  let actual = {};
  for (const key in fields) {
    if (!object.hasOwnProperty(key)) {
      if (fields[key] != undefined) {
        return false;
      }
    }
    actual[key] = object[key];
  }
  return ObjectUtils.deepEqual(actual, fields);
}

add_setup(async function head_initialize() {
  Services.prefs.setBoolPref("extensions.experiments.enabled", true);
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
    Services.prefs.clearUserPref("dom.forms.autocomplete.formautofill");
    Services.prefs.clearUserPref("extensions.formautofill.addresses.enabled");
    Services.prefs.clearUserPref("extensions.formautofill.creditCards.enabled");
  });

  await loadExtension();
});

let OSKeyStoreTestUtils;
add_setup(async function os_key_store_setup() {
  ({ OSKeyStoreTestUtils } = ChromeUtils.importESModule(
    "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
  ));
  OSKeyStoreTestUtils.setup();
  registerCleanupFunction(async function cleanup() {
    await OSKeyStoreTestUtils.cleanup();
  });
});
