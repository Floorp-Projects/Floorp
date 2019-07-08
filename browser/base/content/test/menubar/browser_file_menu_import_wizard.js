add_task(async function file_menu_import_wizard() {
  // We can't call this code directly or our JS execution will get blocked on Windows/Linux where
  // the dialog is modal.
  executeSoon(() =>
    document.getElementById("menu_importFromAnotherBrowser").doCommand()
  );

  await TestUtils.waitForCondition(
    () => Services.wm.getMostRecentWindow("Browser:MigrationWizard"),
    "Migrator window opened"
  );

  let migratorWindow = Services.wm.getMostRecentWindow(
    "Browser:MigrationWizard"
  );
  ok(migratorWindow, "Migrator window opened");

  await BrowserTestUtils.closeWindow(migratorWindow);
});
