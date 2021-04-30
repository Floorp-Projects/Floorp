/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals browser */

/** For use inside an iframe onload function, throws an Error if iframe src is not blank.html

    Should be applied *inside* catcher.watchFunction
*/
this.assertIsBlankDocument = function assertIsBlankDocument(doc) {
  if (doc.documentURI !== browser.runtime.getURL("blank.html")) {
    const exc = new Error("iframe URL does not match expected blank.html");
    exc.foundURL = doc.documentURI;
    throw exc;
  }
};
null;
