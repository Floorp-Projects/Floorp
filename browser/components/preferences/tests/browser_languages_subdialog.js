add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  const contentDocument = gBrowser.contentDocument;
  const dialogOverlay = content.gSubDialog._preloadDialog._overlay;

  async function languagesSubdialogOpened() {
    const promiseSubDialogLoaded = promiseLoadSubDialog(
      "chrome://browser/content/preferences/dialogs/languages.xhtml"
    );
    contentDocument.getElementById("chooseLanguage").click();
    const win = await promiseSubDialogLoaded;
    win.Preferences.forceEnableInstantApply();
    is(dialogOverlay.style.visibility, "visible", "The dialog is visible.");
    return win;
  }

  function closeLanguagesSubdialog() {
    const closeBtn = dialogOverlay.querySelector(".dialogClose");
    closeBtn.doCommand();
  }

  is(dialogOverlay.style.visibility, "", "The dialog is invisible.");
  let win = await languagesSubdialogOpened();
  ok(
    win.document.getElementById("spoofEnglish").hidden,
    "The 'Request English' checkbox is hidden."
  );
  closeLanguagesSubdialog();
  is(dialogOverlay.style.visibility, "", "The dialog is invisible.");

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
  closeLanguagesSubdialog();

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
  closeLanguagesSubdialog();

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
  closeLanguagesSubdialog();

  gBrowser.removeCurrentTab();
});
