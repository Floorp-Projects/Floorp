add_task(async function() {
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
    ok(!BrowserTestUtils.is_hidden(dialogOverlay), "The dialog is visible.");
    return win;
  }

  function acceptLanguagesSubdialog(win) {
    const button = win.document.querySelector("dialog").getButton("accept");
    button.doCommand();
  }

  ok(BrowserTestUtils.is_hidden(dialogOverlay), "The dialog is invisible.");
  let win = await languagesSubdialogOpened();
  ok(
    win.document.getElementById("spoofEnglish").hidden,
    "The 'Request English' checkbox is hidden."
  );
  acceptLanguagesSubdialog(win);
  ok(BrowserTestUtils.is_hidden(dialogOverlay), "The dialog is invisible.");

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

  gBrowser.removeCurrentTab();
});
