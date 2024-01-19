/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FileMigratorBase } = ChromeUtils.importESModule(
  "resource:///modules/FileMigrators.sys.mjs"
);

const DUMMY_FILEMIGRATOR_KEY = "dummy-file-migrator";
const DUMMY_FILEPICKER_TITLE = "Some dummy file picker title";
const DUMMY_FILTER_TITLE = "Some file type";
const DUMMY_EXTENSION_PATTERN = "*.test";
const TEST_FILE_PATH = getTestFilePath("dummy_file.csv");

/**
 * A subclass of FileMigratorBase that doesn't do anything, but
 * is useful for testing.
 *
 * Notably, the `migrate` method is not overridden here. Tests that
 * use this class should use Sinon to stub out the migrate method.
 */
class DummyFileMigrator extends FileMigratorBase {
  static get key() {
    return DUMMY_FILEMIGRATOR_KEY;
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-file-password-csv";
  }

  static get brandImage() {
    return "chrome://branding/content/document.ico";
  }

  get enabled() {
    return true;
  }

  get progressHeaderL10nID() {
    return "migration-passwords-from-file-progress-header";
  }

  get successHeaderL10nID() {
    return "migration-passwords-from-file-success-header";
  }

  async getFilePickerConfig() {
    return Promise.resolve({
      title: DUMMY_FILEPICKER_TITLE,
      filters: [
        {
          title: DUMMY_FILTER_TITLE,
          extensionPattern: DUMMY_EXTENSION_PATTERN,
        },
      ],
    });
  }

  get displayedResourceTypes() {
    return [MigrationWizardConstants.DISPLAYED_RESOURCE_TYPES.PASSWORDS];
  }
}

const { MockFilePicker } = SpecialPowers;

add_setup(async () => {
  // We use MockFilePicker to simulate a native file picker, and prepare it
  // to return a dummy file pointed at TEST_FILE_PATH. The file at
  // TEST_FILE_PATH is not required (nor expected) to exist.
  MockFilePicker.init(window);
  registerCleanupFunction(() => {
    MockFilePicker.cleanup();
  });
});

/**
 * Tests the flow of selecting a file migrator (in this case,
 * the DummyFileMigrator), getting the file picker opened for it,
 * and then passing the path of the selected file to the migrator.
 */
add_task(async function test_file_migration() {
  let migrator = new DummyFileMigrator();
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  // First, use Sinon to insert our DummyFileMigrator as the only available
  // file migrator.
  sandbox.stub(MigrationUtils, "getFileMigrator").callsFake(() => {
    return migrator;
  });
  sandbox.stub(MigrationUtils, "availableFileMigrators").get(() => {
    return [migrator];
  });

  // This is the expected success state that our DummyFileMigrator will
  // return as the final progress update to the migration wizard.
  const SUCCESS_STATE = {
    [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES.PASSWORDS_NEW]:
      "2 added",
    [MigrationWizardConstants.DISPLAYED_FILE_RESOURCE_TYPES.PASSWORDS_UPDATED]:
      "1 updated",
  };

  let migrateStub = sandbox.stub(migrator, "migrate").callsFake(filePath => {
    Assert.equal(filePath, TEST_FILE_PATH);
    return SUCCESS_STATE;
  });

  let dummyFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  dummyFile.initWithPath(TEST_FILE_PATH);
  let filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      Assert.ok(true, "Filepicker shown.");
      MockFilePicker.setFiles([dummyFile]);
      resolve();
    };
  });
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;

    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );

    // Now select our DummyFileMigrator from the list.
    let selector = shadow.querySelector("#browser-profile-selector");
    selector.click();

    info("Waiting for panel-list shown");
    await new Promise(resolve => {
      shadow
        .querySelector("panel-list")
        .addEventListener("shown", resolve, { once: true });
    });

    info("Panel list shown. Clicking on panel-item");
    let panelItem = shadow.querySelector(
      `panel-item[key="${DUMMY_FILEMIGRATOR_KEY}"]`
    );
    panelItem.click();

    // Selecting a file migrator from the selector should automatically
    // open the file picker, so we await it here. Once the file is
    // selected, migration should begin immediately.

    info("Waiting for file picker");
    await filePickerShownPromise;
    await wizardDone;
    Assert.ok(migrateStub.called, "Migrate on DummyFileMigrator was called.");

    // At this point, with migration having completed, we should be showing
    // the PROGRESS page with the SUCCESS_STATE represented.
    let deck = shadow.querySelector("#wizard-deck");
    Assert.equal(
      deck.selectedViewName,
      `page-${MigrationWizardConstants.PAGES.FILE_IMPORT_PROGRESS}`
    );

    // We expect only the displayed resource types in SUCCESS_STATE are
    // displayed now.
    let progressGroups = shadow.querySelectorAll(
      "div[name='page-file-import-progress'] .resource-progress-group"
    );
    for (let progressGroup of progressGroups) {
      let expectedMessageText =
        SUCCESS_STATE[progressGroup.dataset.resourceType];
      if (expectedMessageText) {
        let progressIcon = progressGroup.querySelector(".progress-icon");
        Assert.stringMatches(
          progressIcon.getAttribute("state"),
          "success",
          "Should be showing completed state."
        );

        let messageText =
          progressGroup.querySelector(".message-text").textContent;
        Assert.equal(messageText, expectedMessageText);
      } else {
        Assert.ok(
          BrowserTestUtils.isHidden(progressGroup),
          `Resource progress group for ${progressGroup.dataset.resourceType}` +
            ` should be hidden.`
        );
      }
    }
  });

  sandbox.restore();
});

/**
 * Tests that the migration wizard will go back to the selection page and
 * show an error message if the migration for a FileMigrator throws an
 * exception.
 */
add_task(async function test_file_migration_error() {
  let migrator = new DummyFileMigrator();
  let sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });

  // First, use Sinon to insert our DummyFileMigrator as the only available
  // file migrator.
  sandbox.stub(MigrationUtils, "getFileMigrator").callsFake(() => {
    return migrator;
  });
  sandbox.stub(MigrationUtils, "availableFileMigrators").get(() => {
    return [migrator];
  });

  const ERROR_MESSAGE = "This is my error message";

  let migrateStub = sandbox.stub(migrator, "migrate").callsFake(() => {
    throw new Error(ERROR_MESSAGE);
  });

  let dummyFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  dummyFile.initWithPath(TEST_FILE_PATH);
  let filePickerShownPromise = new Promise(resolve => {
    MockFilePicker.showCallback = () => {
      Assert.ok(true, "Filepicker shown.");
      MockFilePicker.setFiles([dummyFile]);
      resolve();
    };
  });
  MockFilePicker.returnValue = MockFilePicker.returnOK;

  await withMigrationWizardDialog(async prefsWin => {
    let dialogBody = prefsWin.document.body;
    let wizard = dialogBody.querySelector("migration-wizard");
    let shadow = wizard.openOrClosedShadowRoot;

    let wizardDone = BrowserTestUtils.waitForEvent(
      wizard,
      "MigrationWizard:DoneMigration"
    );

    // Now select our DummyFileMigrator from the list.
    let selector = shadow.querySelector("#browser-profile-selector");
    selector.click();

    info("Waiting for panel-list shown");
    await new Promise(resolve => {
      shadow
        .querySelector("panel-list")
        .addEventListener("shown", resolve, { once: true });
    });

    info("Panel list shown. Clicking on panel-item");
    let panelItem = shadow.querySelector(
      `panel-item[key="${DUMMY_FILEMIGRATOR_KEY}"]`
    );
    panelItem.click();

    // Selecting a file migrator from the selector should automatically
    // open the file picker, so we await it here. Once the file is
    // selected, migration should begin immediately.

    info("Waiting for file picker");
    await filePickerShownPromise;
    await wizardDone;
    Assert.ok(migrateStub.called, "Migrate on DummyFileMigrator was called.");

    // At this point, with migration having completed, we should be showing
    // the SELECTION page again with the ERROR_MESSAGE displayed.
    let deck = shadow.querySelector("#wizard-deck");
    await BrowserTestUtils.waitForMutationCondition(
      deck,
      { attributeFilter: ["selected-view"] },
      () => {
        return (
          deck.getAttribute("selected-view") ==
          "page-" + MigrationWizardConstants.PAGES.SELECTION
        );
      }
    );

    Assert.equal(
      selector.selectedPanelItem.getAttribute("key"),
      DUMMY_FILEMIGRATOR_KEY,
      "Should have the file migrator selected."
    );

    let errorMessageContainer = shadow.querySelector(".file-import-error");
    Assert.ok(
      BrowserTestUtils.isVisible(errorMessageContainer),
      "Should be showing the error message container"
    );

    let fileImportErrorMessage = shadow.querySelector(
      "#file-import-error-message"
    ).textContent;
    Assert.equal(fileImportErrorMessage, ERROR_MESSAGE);
  });

  sandbox.restore();
});
