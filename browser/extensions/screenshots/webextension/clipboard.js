/* globals catcher, assertIsBlankDocument */

"use strict";

this.clipboard = (function() {
  let exports = {};

  exports.copy = function(text) {
    return new Promise((resolve, reject) => {
      let element = document.createElement("iframe");
      element.src = browser.extension.getURL("blank.html");
      // We can't actually hide the iframe while copying, but we can make
      // it close to invisible:
      element.style.opacity = "0";
      element.style.width = "1px";
      element.style.height = "1px";
      element.addEventListener("load", catcher.watchFunction(() => {
        try {
          let doc = element.contentDocument;
          assertIsBlankDocument(doc);
          let el = doc.createElement("textarea");
          doc.body.appendChild(el);
          el.value = text;
          if (!text) {
            let exc = new Error("Clipboard copy given empty text");
            exc.noPopup = true;
            catcher.unhandled(exc);
          }
          el.select();
          if (doc.activeElement !== el) {
            let unhandledTag = doc.activeElement ? doc.activeElement.tagName : "No active element";
            let exc = new Error("Clipboard el.select failed");
            exc.activeElement = unhandledTag;
            exc.noPopup = true;
            catcher.unhandled(exc);
          }
          const copied = doc.execCommand("copy");
          if (!copied) {
            catcher.unhandled(new Error("Clipboard copy failed"));
          }
          el.remove();
          resolve(copied);
        } finally {
          element.remove();
        }
      }), {once: true});
      document.body.appendChild(element);
    });
  };

  return exports;
})();
null;
