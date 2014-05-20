/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "TranslationContentHandler" ];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LanguageDetector",
  "resource:///modules/translation/LanguageDetector.jsm");

this.TranslationContentHandler = function(global, docShell) {
  let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebProgress);
  webProgress.addProgressListener(this, Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);

  global.addMessageListener("Translation:TranslateDocument", this);
  global.addMessageListener("Translation:ShowTranslation", this);
  global.addMessageListener("Translation:ShowOriginal", this);
  this.global = global;
}

TranslationContentHandler.prototype = {
  /* nsIWebProgressListener implementation */
  onStateChange: function(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (!aWebProgress.isTopLevel ||
        !(aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) ||
        !this.global.content)
      return;

    let url = aRequest.name;
    if (!url.startsWith("http://") && !url.startsWith("https://"))
      return;

    // Grab a 60k sample of text from the page.
    let encoder = Cc["@mozilla.org/layout/documentEncoder;1?type=text/plain"]
                    .createInstance(Ci.nsIDocumentEncoder);
    encoder.init(this.global.content.document, "text/plain", encoder.SkipInvisibleContent);
    let string = encoder.encodeToStringWithMaxLength(60 * 1024);

    // Language detection isn't reliable on very short strings.
    if (string.length < 100)
      return;

    LanguageDetector.detectLanguage(string).then(result => {
      if (result.confident)
        this.global.sendAsyncMessage("LanguageDetection:Result", result.language);
    });
  },

  // Unused methods.
  onProgressChange: function() {},
  onLocationChange: function() {},
  onStatusChange:   function() {},
  onSecurityChange: function() {},

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  receiveMessage: function(msg) {
    switch (msg.name) {
      case "Translation:TranslateDocument":
      {
        Cu.import("resource:///modules/translation/TranslationDocument.jsm");
        Cu.import("resource:///modules/translation/BingTranslator.jsm");

        let translationDocument = new TranslationDocument(this.global.content.document);
        let bingTranslation = new BingTranslation(translationDocument,
                                                  msg.data.from,
                                                  msg.data.to);

        this.global.content.translationDocument = translationDocument;
        bingTranslation.translate().then(
          success => {
            this.global.sendAsyncMessage("Translation:Finished", {success: true});
            translationDocument.showTranslation();
          },
          error => {
            this.global.sendAsyncMessage("Translation:Finished", {success: false});
          }
        );
        break;
      }

      case "Translation:ShowOriginal":
        this.global.content.translationDocument.showOriginal();
        break;

      case "Translation:ShowTranslation":
        this.global.content.translationDocument.showTranslation();
        break;
    }
  }
};
