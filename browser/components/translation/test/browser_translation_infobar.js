/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the translation infobar, using a fake 'Translation' implementation.

var Translation = {
  supportedSourceLanguages: ["en", "zh", "ja", "es", "de", "fr", "ru", "ar", "ko", "pt"],
  supportedTargetLanguages: ["en", "pl", "tr", "vi"],
  defaultTargetLanguage: "en",

  _translateFrom: "",
  _translateTo: "",
  _deferred: null,
  translate: function(aFrom, aTo) {
    this._translateFrom = aFrom;
    this._translateTo = aTo;
    this._deferred = Promise.defer();
    return this._deferred.promise;
  },

  _reset: function() {
    this._translateFrom = "";
    this._translateTo = "";
    this._deferred = null;
  },

  failTranslation: function() {
    this._deferred.reject();
    this._reset();
  },

  finishTranslation: function() {
    this._deferred.resolve();
    this._reset();
  },

  _showOriginalCalled: false,
  showOriginalContent: function() {
    this._showOriginalCalled = true;
  },

  _showTranslationCalled: false,
  showTranslatedContent: function() {
    this._showTranslationCalled = true;
  },

  showTranslationUI: function(aLanguage) {
    let notificationBox = gBrowser.getNotificationBox();
    let notif = notificationBox.appendNotification("", "translation", null,
                                                   notificationBox.PRIORITY_INFO_HIGH);
    notif.init(this);
    notif.detectedLanguage = aLanguage;
    return notif;
  }
};

function test() {
  waitForExplicitFinish();

  // Show an info bar saying the current page is in French
  let notif = Translation.showTranslationUI("fr");
  is(notif.state, notif.STATE_OFFER, "the infobar is offering translation");
  is(notif._getAnonElt("detectedLanguage").value, "fr", "The detected language is displayed");

  // Click the "Translate" button
  notif._getAnonElt("translate").click();
  is(notif.state, notif.STATE_TRANSLATING, "the infobar is in the translating state");
  ok(!!Translation._deferred, "Translation.translate has been called");
  is(Translation._translateFrom, "fr", "from language correct");
  is(Translation._translateTo, Translation.defaultTargetLanguage, "from language correct");

  // Make the translation fail and check we are in the error state.
  Translation.failTranslation();
  is(notif.state, notif.STATE_ERROR, "infobar in the error state");

  // Click the try again button
  notif._getAnonElt("tryAgain").click();
  is(notif.state, notif.STATE_TRANSLATING, "infobar in the translating state");
  ok(!!Translation._deferred, "Translation.translate has been called");
  is(Translation._translateFrom, "fr", "from language correct");
  is(Translation._translateTo, Translation.defaultTargetLanguage, "to language correct");

  // Make the translation succeed and check we are in the 'translated' state.
  Translation.finishTranslation();
  is(notif.state, notif.STATE_TRANSLATED, "infobar in the translated state");

  // Test 'Show Original' / 'Show Translation' buttons.
  // First check 'Show Original' is visible and 'Show Translation' is hidden.
  ok(!notif._getAnonElt("showOriginal").hidden, "'Show Original' button visible");
  ok(notif._getAnonElt("showTranslation").hidden, "'Show Translation' button hidden");
  // Click the button.
  notif._getAnonElt("showOriginal").click();
  // Check the correct function has been called.
  ok(Translation._showOriginalCalled, "'Translation.showOriginalContent' called")
  ok(!Translation._showTranslationCalled, "'Translation.showTranslatedContent' not called")
  Translation._showOriginalCalled = false;
  // And the 'Show Translation' button is now visible.
  ok(notif._getAnonElt("showOriginal").hidden, "'Show Original' button hidden");
  ok(!notif._getAnonElt("showTranslation").hidden, "'Show Translation' button visible");
  // Click the 'Show Translation' button
  notif._getAnonElt("showTranslation").click();
  // Check the correct function has been called.
  ok(!Translation._showOriginalCalled, "'Translation.showOriginalContent' not called")
  ok(Translation._showTranslationCalled, "'Translation.showTranslatedContent' called")
  Translation._showTranslationCalled = false;
  // Check that the 'Show Original' button is visible again.
  ok(!notif._getAnonElt("showOriginal").hidden, "'Show Original' button visible");
  ok(notif._getAnonElt("showTranslation").hidden, "'Show Translation' button hidden");

  // Check that changing the source language causes a re-translation
  let from = notif._getAnonElt("fromLanguage");
  from.value = "es";
  from.doCommand();
  is(notif.state, notif.STATE_TRANSLATING, "infobar in the translating state");
  ok(!!Translation._deferred, "Translation.translate has been called");
  is(Translation._translateFrom, "es", "from language correct");
  is(Translation._translateTo, Translation.defaultTargetLanguage, "to language correct");
  Translation.finishTranslation();

  // Check that changing the target language causes a re-translation
  let to = notif._getAnonElt("toLanguage");
  to.value = "pl";
  to.doCommand();
  is(notif.state, notif.STATE_TRANSLATING, "infobar in the translating state");
  ok(!!Translation._deferred, "Translation.translate has been called");
  is(Translation._translateFrom, "es", "from language correct");
  is(Translation._translateTo, "pl", "to language correct");
  Translation.finishTranslation();

  // Cleanup.
  notif.close();

  // Reopen the info bar to check that it's possible to override the detected language.
  notif = Translation.showTranslationUI("fr");
  is(notif.state, notif.STATE_OFFER, "the infobar is offering translation");
  is(notif._getAnonElt("detectedLanguage").value, "fr", "The detected language is displayed");
  // Change the language and click 'Translate'
  notif._getAnonElt("detectedLanguage").value = "ja";
  notif._getAnonElt("translate").click();
  is(notif.state, notif.STATE_TRANSLATING, "the infobar is in the translating state");
  ok(!!Translation._deferred, "Translation.translate has been called");
  is(Translation._translateFrom, "ja", "from language correct");
  notif.close();

  // Reopen one last time to check the 'Not Now' button closes the notification.
  notif = Translation.showTranslationUI("fr");

  let notificationBox = gBrowser.getNotificationBox();
  ok(!!notificationBox.getNotificationWithValue("translation"), "there's a 'translate' notification");
  notif._getAnonElt("notNow").click();
  ok(!notificationBox.getNotificationWithValue("translation"), "no 'translate' notification after clicking 'not now'");

  finish();
}
