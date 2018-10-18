/* globals catcher, assertIsBlankDocument */

"use strict";

this.clipboard = (function() {
  const exports = {};

  exports.copy = function(text) {
    return new Promise((resolve, reject) => {
      const element = document.createElement("iframe");
      element.src = browser.extension.getURL("blank.html");
      // We can't actually hide the iframe while copying, but we can make
      // it close to invisible:
      element.style.opacity = "0";
      element.style.width = "1px";
      element.style.height = "1px";
      element.style.display = "block";
      element.addEventListener("load", catcher.watchFunction(() => {
        try {
          const doc = element.contentDocument;
          assertIsBlankDocument(doc);
          const el = doc.createElement("textarea");
          doc.body.appendChild(el);
          el.value = text;
          if (!text) {
            const exc = new Error("Clipboard copy given empty text");
            exc.noPopup = true;
            catcher.unhandled(exc);
          }
          el.select();
          if (doc.activeElement !== el) {
            const unhandledTag = doc.activeElement ? doc.activeElement.tagName : "No active element";
            const exc = new Error("Clipboard el.select failed");
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
