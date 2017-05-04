/* globals catcher */

"use strict";

this.clipboard = (function() {
  let exports = {};

  exports.copy = function(text) {
    let el = document.createElement("textarea");
    document.body.appendChild(el);
    el.value = text;
    el.select();
    const copied = document.execCommand("copy");
    document.body.removeChild(el);
    if (!copied) {
      catcher.unhandled(new Error("Clipboard copy failed"));
    }
    return copied;
  };

  return exports;
})();
null;
