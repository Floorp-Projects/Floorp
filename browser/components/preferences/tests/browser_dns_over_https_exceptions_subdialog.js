async function dohExceptionsSubdialogOpened(dialogOverlay) {
  const promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/dohExceptions.xhtml"
  );
  const contentDocument = gBrowser.contentDocument;
  contentDocument.getElementById("dohExceptionsButton").click();
  const win = await promiseSubDialogLoaded;
  dialogOverlay = content.gSubDialog._topDialog._overlay;
  ok(!BrowserTestUtils.isHidden(dialogOverlay), "The dialog is visible.");
  return win;
}

function acceptDoHExceptionsSubdialog(win) {
  const button = win.document.querySelector("dialog").getButton("accept");
  button.doCommand();
}

function cancelDoHExceptionsSubdialog(win) {
  const button = win.document.querySelector("dialog").getButton("cancel");
  button.doCommand();
}

function addNewException(domain, dialog) {
  let url = dialog.document.getElementById("url");
  let addButton = dialog.document.getElementById("btnAddException");

  ok(
    addButton.disabled,
    "The Add button is disabled when domain's input box is empty"
  );

  url.focus();
  EventUtils.sendString(domain);

  ok(
    !addButton.disabled,
    "The Add button is enabled when some text is on domain's input box"
  );

  addButton.click();

  is(
    url.value,
    "",
    "Domain input box is empty after adding a new domain to the list"
  );
  ok(
    addButton.disabled,
    "The Add button is disabled after exception has been added to the list"
  );
}

add_task(async function () {
  Services.prefs.lockPref("network.trr.excluded-domains");

  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  let dialogOverlay = content.gSubDialog._preloadDialog._overlay;
  let win = await dohExceptionsSubdialogOpened(dialogOverlay);

  ok(
    win.document.getElementById("btnAddException").disabled,
    "The Add button is disabled when preference is locked"
  );
  ok(
    win.document.getElementById("url").disabled,
    "The url input box is disabled when preference is locked"
  );

  cancelDoHExceptionsSubdialog(win);
  Services.prefs.unlockPref("network.trr.excluded-domains");
  win = await dohExceptionsSubdialogOpened(dialogOverlay);

  ok(
    win.document.getElementById("btnAddException").disabled,
    "The Add button is disabled when preference is not locked"
  );
  ok(
    !win.document.getElementById("url").disabled,
    "The url input box is enabled when preference is not locked"
  );

  cancelDoHExceptionsSubdialog(win);
  gBrowser.removeCurrentTab();
});

add_task(async function () {
  await openPreferencesViaOpenPreferencesAPI("panePrivacy", {
    leaveOpen: true,
  });
  let dialogOverlay = content.gSubDialog._preloadDialog._overlay;

  ok(BrowserTestUtils.isHidden(dialogOverlay), "The dialog is invisible.");
  let win = await dohExceptionsSubdialogOpened(dialogOverlay);
  acceptDoHExceptionsSubdialog(win);
  ok(BrowserTestUtils.isHidden(dialogOverlay), "The dialog is invisible.");

  win = await dohExceptionsSubdialogOpened(dialogOverlay);
  Assert.equal(
    win.document.getElementById("permissionsBox").itemCount,
    0,
    "There are no exceptions set."
  );
  ok(
    win.document.getElementById("removeException").disabled,
    "The Remove button is disabled when there are no exceptions on the list"
  );
  ok(
    win.document.getElementById("removeAllExceptions").disabled,
    "The Remove All button is disabled when there are no exceptions on the list"
  );
  ok(
    win.document.getElementById("btnAddException").disabled,
    "The Add button is disabled when dialog box has just been opened"
  );

  addNewException("test1.com", win);
  Assert.equal(
    win.document.getElementById("permissionsBox").itemCount,
    1,
    "List shows 1 new item"
  );
  let activeExceptions = win.document.getElementById("permissionsBox").children;
  is(
    activeExceptions[0].getAttribute("domain"),
    "test1.com",
    "test1.com added to the list"
  );
  ok(
    !win.document.getElementById("removeAllExceptions").disabled,
    "The Remove All button is enabled when there is one exception on the list"
  );
  addNewException("test2.com", win);
  addNewException("test3.com", win);
  Assert.equal(
    win.document.getElementById("permissionsBox").itemCount,
    3,
    "List shows 3 domain items"
  );
  ok(
    win.document.getElementById("removeException").disabled,
    "The Remove button is disabled when no exception has been selected"
  );
  win.document.getElementById("permissionsBox").selectedIndex = 1;
  ok(
    !win.document.getElementById("removeException").disabled,
    "The Remove button is enabled when an exception has been selected"
  );
  win.document.getElementById("removeException").doCommand();
  Assert.equal(
    win.document.getElementById("permissionsBox").itemCount,
    2,
    "List shows 2 domain items after removing one of the three"
  );
  activeExceptions = win.document.getElementById("permissionsBox").children;
  ok(
    win.document.getElementById("permissionsBox").itemCount == 2 &&
      activeExceptions[0].getAttribute("domain") == "test1.com" &&
      activeExceptions[1].getAttribute("domain") == "test3.com",
    "test1.com and test3.com are the only items left on the list"
  );
  is(
    win.document.getElementById("permissionsBox").selectedIndex,
    -1,
    "There is no selected item after removal"
  );
  addNewException("test2.com", win);
  activeExceptions = win.document.getElementById("permissionsBox").children;
  ok(
    win.document.getElementById("permissionsBox").itemCount == 3 &&
      activeExceptions[1].getAttribute("domain") == "test2.com",
    "test2.com has been added as the second item"
  );
  win.document.getElementById("removeAllExceptions").doCommand();
  is(
    win.document.getElementById("permissionsBox").itemCount,
    0,
    "There are no elements on the list after clicking Remove All"
  );

  acceptDoHExceptionsSubdialog(win);

  gBrowser.removeCurrentTab();
});
