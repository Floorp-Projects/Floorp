add_task(async function () {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  const contentDocument = gBrowser.contentDocument;
  let dialogOverlay = content.gSubDialog._preloadDialog._overlay;

  async function languagesSubdialogOpened() {
    const promiseSubDialogLoaded = promiseLoadSubDialog(
      "chrome://browser/content/preferences/dialogs/languages.xhtml"
    );
    contentDocument.getElementById("chooseLanguage").click();
    const win = await promiseSubDialogLoaded;
    dialogOverlay = content.gSubDialog._topDialog._overlay;
    ok(!BrowserTestUtils.isHidden(dialogOverlay), "The dialog is visible.");
    return win;
  }

  function acceptLanguagesSubdialog(win) {
    const button = win.document.querySelector("dialog").getButton("accept");
    button.doCommand();
  }

  ok(BrowserTestUtils.isHidden(dialogOverlay), "The dialog is invisible.");
  let win = await languagesSubdialogOpened();
  ok(
    win.document.getElementById("spoofEnglish").hidden,
    "The 'Request English' checkbox is hidden."
  );
  acceptLanguagesSubdialog(win);
  ok(BrowserTestUtils.isHidden(dialogOverlay), "The dialog is invisible.");

  await SpecialPowers.pushPrefEnv({
    set: [["intl.accept_languages", "en-US,en-XX,foo"]],
  });
  win = await languagesSubdialogOpened();
  let activeLanguages = win.document.getElementById("activeLanguages").children;
  Assert.equal(
    activeLanguages[0].id,
    "en-us",
    "The ID for 'en-US' locale code is correctly set."
  );
  Assert.equal(
    activeLanguages[0].firstChild.value,
    "English (United States) [en-us]",
    "The name for known 'en-US' locale code is correctly resolved."
  );
  Assert.equal(
    activeLanguages[1].id,
    "en-xx",
    "The ID for 'en-XX' locale code is correctly set."
  );
  Assert.equal(
    activeLanguages[1].firstChild.value,
    "English [en-xx]",
    "The name for unknown 'en-XX' locale code is resolved using 'en'."
  );
  Assert.equal(
    activeLanguages[2].firstChild.value,
    " [foo]",
    "The name for unknown 'foo' locale code is empty."
  );
  acceptLanguagesSubdialog(win);
  await SpecialPowers.popPrefEnv();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      ["privacy.spoof_english", 0],
    ],
  });

  win = await languagesSubdialogOpened();
  ok(
    !win.document.getElementById("spoofEnglish").hidden,
    "The 'Request English' checkbox isn't hidden."
  );
  ok(
    !win.document.getElementById("spoofEnglish").checked,
    "The 'Request English' checkbox isn't checked."
  );
  is(
    win.Preferences.get("privacy.spoof_english").value,
    0,
    "The privacy.spoof_english pref is set to 0."
  );

  win.document.getElementById("spoofEnglish").checked = true;
  win.document.getElementById("spoofEnglish").doCommand();
  ok(
    win.document.getElementById("spoofEnglish").checked,
    "The 'Request English' checkbox is checked."
  );
  is(
    win.Preferences.get("privacy.spoof_english").value,
    2,
    "The privacy.spoof_english pref is set to 2."
  );
  acceptLanguagesSubdialog(win);

  win = await languagesSubdialogOpened();
  ok(
    !win.document.getElementById("spoofEnglish").hidden,
    "The 'Request English' checkbox isn't hidden."
  );
  ok(
    win.document.getElementById("spoofEnglish").checked,
    "The 'Request English' checkbox is checked."
  );
  is(
    win.Preferences.get("privacy.spoof_english").value,
    2,
    "The privacy.spoof_english pref is set to 2."
  );

  win.document.getElementById("spoofEnglish").checked = false;
  win.document.getElementById("spoofEnglish").doCommand();
  ok(
    !win.document.getElementById("spoofEnglish").checked,
    "The 'Request English' checkbox isn't checked."
  );
  is(
    win.Preferences.get("privacy.spoof_english").value,
    1,
    "The privacy.spoof_english pref is set to 1."
  );
  acceptLanguagesSubdialog(win);

  win = await languagesSubdialogOpened();
  ok(
    !win.document.getElementById("spoofEnglish").hidden,
    "The 'Request English' checkbox isn't hidden."
  );
  ok(
    !win.document.getElementById("spoofEnglish").checked,
    "The 'Request English' checkbox isn't checked."
  );
  is(
    win.Preferences.get("privacy.spoof_english").value,
    1,
    "The privacy.spoof_english pref is set to 1."
  );
  acceptLanguagesSubdialog(win);
  await SpecialPowers.popPrefEnv();

  await SpecialPowers.pushPrefEnv({
    set: [["intl.accept_languages", "en-US"]],
  });
  win = await languagesSubdialogOpened();
  is(
    win.document.getElementById("remove").disabled,
    false,
    "The Remove button is enabled when any language is selected on the list"
  );

  win.document.getElementById("remove").doCommand();
  is(
    win.Preferences.get("intl.accept_languages").value,
    "",
    "Accepted language has been removed from the preference"
  );
  is(
    win.document.getElementById("activeLanguages").itemCount,
    0,
    "Accepted languages list is empty"
  );
  ok(
    win.document.getElementById("remove").disabled,
    "The Remove button is disabled when there is no language on the list"
  );
  acceptLanguagesSubdialog(win);

  gBrowser.removeCurrentTab();
});
