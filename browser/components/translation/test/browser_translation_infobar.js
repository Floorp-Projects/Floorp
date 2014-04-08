/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the translation infobar, using a fake 'Translation' implementation.

Components.utils.import("resource:///modules/translation/Translation.jsm");

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

var TranslationStub = {
  __proto__: Translation.prototype,

  translate: function(aFrom, aTo) {
    this.state = this.STATE_TRANSLATING;
    this.translatedFrom = aFrom;
    this.translatedTo = aTo;
  },

  _reset: function() {
    this.translatedFrom = "";
    this.translatedTo = "";
  },

  failTranslation: function() {
    this.state = this.STATE_ERROR;
    this._reset();
  },

  finishTranslation: function() {
    this.showTranslatedContent();
    this.state = this.STATE_TRANSLATED;
    this._reset();
  }
};


function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("load", function onload() {
    tab.linkedBrowser.removeEventListener("load", onload, true);
    TranslationStub.browser = gBrowser.selectedBrowser;
    registerCleanupFunction(function () {
      gBrowser.removeTab(tab);
    });
    run_tests(() => {
      finish();
    });
  }, true);

  content.location = "data:text/plain,test page";
}

function checkURLBarIcon(aExpectTranslated = false) {
  is(!PopupNotifications.getNotification("translate"), aExpectTranslated,
     "translate icon " + (aExpectTranslated ? "not " : "") + "shown");
  is(!!PopupNotifications.getNotification("translated"), aExpectTranslated,
     "translated icon " + (aExpectTranslated ? "" : "not ") + "shown");
}

function run_tests(aFinishCallback) {
  info("Show an info bar saying the current page is in French");
  let notif = TranslationStub.showTranslationUI("fr");
  is(notif.state, TranslationStub.STATE_OFFER, "the infobar is offering translation");
  is(notif._getAnonElt("detectedLanguage").value, "fr", "The detected language is displayed");
  checkURLBarIcon();

  info("Click the 'Translate' button");
  notif._getAnonElt("translate").click();
  is(notif.state, TranslationStub.STATE_TRANSLATING, "the infobar is in the translating state");
  ok(!!TranslationStub.translatedFrom, "Translation.translate has been called");
  is(TranslationStub.translatedFrom, "fr", "from language correct");
  is(TranslationStub.translatedTo, TranslationStub.defaultTargetLanguage, "from language correct");
  checkURLBarIcon();

  info("Make the translation fail and check we are in the error state.");
  TranslationStub.failTranslation();
  is(notif.state, TranslationStub.STATE_ERROR, "infobar in the error state");
  checkURLBarIcon();

  info("Click the try again button");
  notif._getAnonElt("tryAgain").click();
  is(notif.state, TranslationStub.STATE_TRANSLATING, "infobar in the translating state");
  ok(!!TranslationStub.translatedFrom, "Translation.translate has been called");
  is(TranslationStub.translatedFrom, "fr", "from language correct");
  is(TranslationStub.translatedTo, TranslationStub.defaultTargetLanguage, "from language correct");
  checkURLBarIcon();

  info("Make the translation succeed and check we are in the 'translated' state.");
  TranslationStub.finishTranslation();
  is(notif.state, TranslationStub.STATE_TRANSLATED, "infobar in the translated state");
  checkURLBarIcon(true);

  info("Test 'Show original' / 'Show Translation' buttons.");
  // First check 'Show Original' is visible and 'Show Translation' is hidden.
  ok(!notif._getAnonElt("showOriginal").hidden, "'Show Original' button visible");
  ok(notif._getAnonElt("showTranslation").hidden, "'Show Translation' button hidden");
  // Click the button.
  notif._getAnonElt("showOriginal").click();
  // Check that the url bar icon shows the original content is displayed.
  checkURLBarIcon();
  // And the 'Show Translation' button is now visible.
  ok(notif._getAnonElt("showOriginal").hidden, "'Show Original' button hidden");
  ok(!notif._getAnonElt("showTranslation").hidden, "'Show Translation' button visible");
  // Click the 'Show Translation' button
  notif._getAnonElt("showTranslation").click();
  // Check that the url bar icon shows the page is translated.
  checkURLBarIcon(true);
  // Check that the 'Show Original' button is visible again.
  ok(!notif._getAnonElt("showOriginal").hidden, "'Show Original' button visible");
  ok(notif._getAnonElt("showTranslation").hidden, "'Show Translation' button hidden");

  info("Check that changing the source language causes a re-translation");
  let from = notif._getAnonElt("fromLanguage");
  from.value = "es";
  from.doCommand();
  is(notif.state, TranslationStub.STATE_TRANSLATING, "infobar in the translating state");
  ok(!!TranslationStub.translatedFrom, "Translation.translate has been called");
  is(TranslationStub.translatedFrom, "es", "from language correct");
  is(TranslationStub.translatedTo, TranslationStub.defaultTargetLanguage, "to language correct");
  // We want to show the 'translated' icon while re-translating,
  // because we are still displaying the previous translation.
  checkURLBarIcon(true);
  TranslationStub.finishTranslation();
  checkURLBarIcon(true);

  info("Check that changing the target language causes a re-translation");
  let to = notif._getAnonElt("toLanguage");
  to.value = "pl";
  to.doCommand();
  is(notif.state, TranslationStub.STATE_TRANSLATING, "infobar in the translating state");
  ok(!!TranslationStub.translatedFrom, "Translation.translate has been called");
  is(TranslationStub.translatedFrom, "es", "from language correct");
  is(TranslationStub.translatedTo, "pl", "to language correct");
  checkURLBarIcon(true);
  TranslationStub.finishTranslation();
  checkURLBarIcon(true);

  // Cleanup.
  notif.close();

  info("Reopen the info bar to check that it's possible to override the detected language.");
  notif = TranslationStub.showTranslationUI("fr");
  is(notif.state, TranslationStub.STATE_OFFER, "the infobar is offering translation");
  is(notif._getAnonElt("detectedLanguage").value, "fr", "The detected language is displayed");
  // Change the language and click 'Translate'
  notif._getAnonElt("detectedLanguage").value = "ja";
  notif._getAnonElt("translate").click();
  is(notif.state, TranslationStub.STATE_TRANSLATING, "the infobar is in the translating state");
  ok(!!TranslationStub.translatedFrom, "Translation.translate has been called");
  is(TranslationStub.translatedFrom, "ja", "from language correct");
  notif.close();

  info("Reopen to check the 'Not Now' button closes the notification.");
  notif = TranslationStub.showTranslationUI("fr");
  let notificationBox = gBrowser.getNotificationBox();
  ok(!!notificationBox.getNotificationWithValue("translation"), "there's a 'translate' notification");
  notif._getAnonElt("notNow").click();
  ok(!notificationBox.getNotificationWithValue("translation"), "no 'translate' notification after clicking 'not now'");

  info("Check that clicking the url bar icon reopens the info bar");
  checkURLBarIcon();
  // Clicking the anchor element causes a 'showing' event to be sent
  // asynchronously to our callback that will then show the infobar.
  PopupNotifications.getNotification("translate").anchorElement.click();
  waitForCondition(() => !!notificationBox.getNotificationWithValue("translation"), () => {
    ok(!!notificationBox.getNotificationWithValue("translation"),
       "there's a 'translate' notification");
    aFinishCallback();
  }, "timeout waiting for the info bar to reappear");
}
