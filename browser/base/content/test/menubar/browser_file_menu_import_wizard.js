add_task(async function file_menu_import_wizard() {
  // We can't call this code directly or our JS execution will get blocked on Windows/Linux where
  // the dialog is modal.
  executeSoon(() =>
    document.getElementById("menu_importFromAnotherBrowser").doCommand()
  );

  await TestUtils.waitForCondition(() => {
    let win = Services.wm.getMostRecentWindow("Browser:MigrationWizard");
    return win && win.document && win.document.readyState == "complete";
  }, "Migrator window loaded");

  let migratorWindow = Services.wm.getMostRecentWindow(
    "Browser:MigrationWizard"
  );
  ok(migratorWindow, "Migrator window opened");

  await BrowserTestUtils.closeWindow(migratorWindow);
});
