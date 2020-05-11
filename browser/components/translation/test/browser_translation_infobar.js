/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the translation infobar, using a fake 'Translation' implementation.

var tmp = {};
ChromeUtils.import(
  "resource:///modules/translation/TranslationParent.jsm",
  tmp
);
var { Translation, TranslationParent } = tmp;

const kDetectLanguagePref = "browser.translation.detectLanguage";
const kShowUIPref = "browser.translation.ui.show";

const text =
  "Il y a aujourd'hui trois cent quarante-huit ans six mois et dix-neuf jours que les Parisiens s'éveillèrent au bruit de toutes les cloches sonnant à grande volée dans la triple enceinte de la Cité, de l'Université et de la Ville.";
const EXAMPLE_URL =
  "http://example.com/document-builder.sjs?html=<html><body>" +
  text +
  "</body></html>";

// Create a subclass that overrides the translation functions. This can be
// instantiated separately from the normal actor creation process. This will
// allow testing translations even when the browser.translation.detectLanguage
// preference is disabled.
class TranslationStub extends TranslationParent {
  constructor(browser) {
    super();
    this._browser = browser;
    this.actorCreated();
  }

  get browser() {
    return this._browser;
  }

  sendAsyncMessage(name, data) {}

  translate(aFrom, aTo) {
    this.state = Translation.STATE_TRANSLATING;
    this.translatedFrom = aFrom;
    this.translatedTo = aTo;
  }

  _reset() {
    this.translatedFrom = "";
    this.translatedTo = "";
  }

  failTranslation() {
    this.state = Translation.STATE_ERROR;
    this._reset();
  }

  finishTranslation() {
    this.showTranslatedContent();
    this.state = Translation.STATE_TRANSLATED;
    this._reset();
  }
}

function showTranslationUI(aDetectedLanguage) {
  let browser = gBrowser.selectedBrowser;
  let translation = new TranslationStub(browser);
  translation.documentStateReceived({
    state: Translation.STATE_OFFER,
    originalShown: true,
    detectedLanguage: aDetectedLanguage,
  });
  return translation.notificationBox.getNotificationWithValue("translation");
}

function hasTranslationInfoBar() {
  return !!gBrowser
    .getNotificationBox()
    .getNotificationWithValue("translation");
}

function checkURLBarIcon(aExpectTranslated = false) {
  is(
    !PopupNotifications.getNotification("translate"),
    aExpectTranslated,
    "translate icon " + (aExpectTranslated ? "not " : "") + "shown"
  );
  is(
    !!PopupNotifications.getNotification("translated"),
    aExpectTranslated,
    "translated icon " + (aExpectTranslated ? "" : "not ") + "shown"
  );
}

add_task(async function test_infobar() {
  await SpecialPowers.pushPrefEnv({
    set: [[kShowUIPref, true]],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/plain,test page"
  );

  TranslationStub.browser = gBrowser.selectedBrowser;

  info("Show an info bar saying the current page is in French");
  let notif = showTranslationUI("fr");
  is(
    notif.state,
    Translation.STATE_OFFER,
    "the infobar is offering translation"
  );
  is(
    notif._getAnonElt("detectedLanguage").value,
    "fr",
    "The detected language is displayed"
  );
  checkURLBarIcon();

  info("Click the 'Translate' button");
  notif._getAnonElt("translate").click();
  is(
    notif.state,
    Translation.STATE_TRANSLATING,
    "the infobar is in the translating state"
  );
  ok(
    !!notif.translation.translatedFrom,
    "Translation.translate has been called"
  );
  is(notif.translation.translatedFrom, "fr", "from language correct");
  is(
    notif.translation.translatedTo,
    Translation.defaultTargetLanguage,
    "from language correct"
  );
  checkURLBarIcon();

  info("Make the translation fail and check we are in the error state.");
  notif.translation.failTranslation();
  is(notif.state, Translation.STATE_ERROR, "infobar in the error state");
  checkURLBarIcon();

  info("Click the try again button");
  notif._getAnonElt("tryAgain").click();
  is(
    notif.state,
    Translation.STATE_TRANSLATING,
    "infobar in the translating state"
  );
  ok(
    !!notif.translation.translatedFrom,
    "Translation.translate has been called"
  );
  is(notif.translation.translatedFrom, "fr", "from language correct");
  is(
    notif.translation.translatedTo,
    Translation.defaultTargetLanguage,
    "from language correct"
  );
  checkURLBarIcon();

  info(
    "Make the translation succeed and check we are in the 'translated' state."
  );
  notif.translation.finishTranslation();
  is(
    notif.state,
    Translation.STATE_TRANSLATED,
    "infobar in the translated state"
  );
  checkURLBarIcon(true);

  info("Test 'Show original' / 'Show Translation' buttons.");
  // First check 'Show Original' is visible and 'Show Translation' is hidden.
  ok(
    !notif._getAnonElt("showOriginal").hidden,
    "'Show Original' button visible"
  );
  ok(
    notif._getAnonElt("showTranslation").hidden,
    "'Show Translation' button hidden"
  );
  // Click the button.
  notif._getAnonElt("showOriginal").click();
  // Check that the url bar icon shows the original content is displayed.
  checkURLBarIcon();
  // And the 'Show Translation' button is now visible.
  ok(notif._getAnonElt("showOriginal").hidden, "'Show Original' button hidden");
  ok(
    !notif._getAnonElt("showTranslation").hidden,
    "'Show Translation' button visible"
  );
  // Click the 'Show Translation' button
  notif._getAnonElt("showTranslation").click();
  // Check that the url bar icon shows the page is translated.
  checkURLBarIcon(true);
  // Check that the 'Show Original' button is visible again.
  ok(
    !notif._getAnonElt("showOriginal").hidden,
    "'Show Original' button visible"
  );
  ok(
    notif._getAnonElt("showTranslation").hidden,
    "'Show Translation' button hidden"
  );

  info("Check that changing the source language causes a re-translation");
  let from = notif._getAnonElt("fromLanguage");
  from.value = "es";
  from.doCommand();
  is(
    notif.state,
    Translation.STATE_TRANSLATING,
    "infobar in the translating state"
  );
  ok(
    !!notif.translation.translatedFrom,
    "Translation.translate has been called"
  );
  is(notif.translation.translatedFrom, "es", "from language correct");
  is(
    notif.translation.translatedTo,
    Translation.defaultTargetLanguage,
    "to language correct"
  );
  // We want to show the 'translated' icon while re-translating,
  // because we are still displaying the previous translation.
  checkURLBarIcon(true);
  notif.translation.finishTranslation();
  checkURLBarIcon(true);

  info("Check that changing the target language causes a re-translation");
  let to = notif._getAnonElt("toLanguage");
  to.value = "pl";
  to.doCommand();
  is(
    notif.state,
    Translation.STATE_TRANSLATING,
    "infobar in the translating state"
  );
  ok(
    !!notif.translation.translatedFrom,
    "Translation.translate has been called"
  );
  is(notif.translation.translatedFrom, "es", "from language correct");
  is(notif.translation.translatedTo, "pl", "to language correct");
  checkURLBarIcon(true);
  notif.translation.finishTranslation();
  checkURLBarIcon(true);

  // Cleanup.
  notif.close();

  info(
    "Reopen the info bar to check that it's possible to override the detected language."
  );
  notif = showTranslationUI("fr");
  is(
    notif.state,
    Translation.STATE_OFFER,
    "the infobar is offering translation"
  );
  is(
    notif._getAnonElt("detectedLanguage").value,
    "fr",
    "The detected language is displayed"
  );
  // Change the language and click 'Translate'
  notif._getAnonElt("detectedLanguage").value = "ja";
  notif._getAnonElt("translate").click();
  is(
    notif.state,
    Translation.STATE_TRANSLATING,
    "the infobar is in the translating state"
  );
  ok(
    !!notif.translation.translatedFrom,
    "Translation.translate has been called"
  );
  is(notif.translation.translatedFrom, "ja", "from language correct");
  notif.close();

  info("Reopen to check the 'Not Now' button closes the notification.");
  notif = showTranslationUI("fr");
  is(hasTranslationInfoBar(), true, "there's a 'translate' notification");
  notif._getAnonElt("notNow").click();
  is(
    hasTranslationInfoBar(),
    false,
    "no 'translate' notification after clicking 'not now'"
  );

  info("Reopen to check the url bar icon closes the notification.");
  notif = showTranslationUI("fr");
  is(hasTranslationInfoBar(), true, "there's a 'translate' notification");
  PopupNotifications.getNotification("translate").anchorElement.click();
  is(
    hasTranslationInfoBar(),
    false,
    "no 'translate' notification after clicking the url bar icon"
  );

  info("Check that clicking the url bar icon reopens the info bar");
  checkURLBarIcon();
  // Clicking the anchor element causes a 'showing' event to be sent
  // asynchronously to our callback that will then show the infobar.
  PopupNotifications.getNotification("translate").anchorElement.click();

  await BrowserTestUtils.waitForCondition(
    hasTranslationInfoBar,
    "timeout waiting for the info bar to reappear"
  );

  ok(hasTranslationInfoBar(), "there's a 'translate' notification");

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_infobar_using_page() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [kDetectLanguagePref, true],
      [kShowUIPref, true],
    ],
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, EXAMPLE_URL);

  await BrowserTestUtils.waitForCondition(
    hasTranslationInfoBar,
    "timeout waiting for the info bar to reappear"
  );

  let notificationBox = gBrowser.getNotificationBox(tab.linkedBrowser);
  let notif = notificationBox.getNotificationWithValue("translation");
  is(
    notif.state,
    Translation.STATE_OFFER,
    "the infobar is offering translation"
  );
  is(
    notif._getAnonElt("detectedLanguage").value,
    "fr",
    "The detected language is displayed"
  );
  checkURLBarIcon();

  await BrowserTestUtils.removeTab(tab);
});
