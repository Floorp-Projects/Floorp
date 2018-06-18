/** For use inside an iframe onload function, throws an Error if iframe src is not blank.html

    Should be applied *inside* catcher.watchFunction
*/
this.assertIsBlankDocument = function assertIsBlankDocument(doc) {
  if (doc.documentURI !== browser.extension.getURL("blank.html")) {
    const exc = new Error("iframe URL does not match expected blank.html");
    exc.foundURL = doc.documentURI;
    throw exc;
  }
};
null;
