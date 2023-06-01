/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1739489 - Entering an emoji using the MacOS IME "crashes" Draft.js editors.
 */

/* globals exportFunction */

console.info(
  "textInput event has been remapped to beforeinput for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1739489 for details."
);

window.wrappedJSObject.TextEvent = window.wrappedJSObject.InputEvent;

const { CustomEvent, Event, EventTarget } = window.wrappedJSObject;
var Remapped = [
  [CustomEvent, "constructor"],
  [Event, "constructor"],
  [Event, "initEvent"],
  [EventTarget, "addEventListener"],
  [EventTarget, "removeEventListener"],
];

for (const [obj, name] of Remapped) {
  const { prototype } = obj;
  const orig = prototype[name];
  Object.defineProperty(prototype, name, {
    value: exportFunction(function (type, b, c, d) {
      if (type?.toLowerCase() === "textinput") {
        type = "beforeinput";
      }
      return orig.call(this, type, b, c, d);
    }, window),
  });
}

if (location.host === "www.reddit.com") {
  (function () {
    const EditorCSS = ".public-DraftEditor-content[contenteditable=true]";
    let obsEditor, obsStart, obsText, obsKey, observer;
    const obsConfig = { characterData: true, childList: true, subtree: true };
    const obsHandler = () => {
      observer.disconnect();
      const finalTextNode = obsEditor.querySelector(
        `[data-offset-key="${obsKey}"] [data-text='true']`
      ).firstChild;
      const end = obsStart + obsText.length;
      window
        .getSelection()
        .setBaseAndExtent(finalTextNode, end, finalTextNode, end);
    };
    observer = new MutationObserver(obsHandler);

    document.documentElement.addEventListener(
      "beforeinput",
      e => {
        if (e.inputType != "insertFromPaste") {
          return;
        }
        const { target } = e;
        obsEditor = target.closest(EditorCSS);
        if (!obsEditor) {
          return;
        }
        const items = e?.dataTransfer.items;
        for (let item of items) {
          if (item.type === "text/plain") {
            e.preventDefault();
            item.getAsString(text => {
              obsText = text;

              // find the editor-managed <span> which contains the text node the
              // cursor starts on, and the cursor's location (or the selection start)
              const sel = window.getSelection();
              obsStart = sel.anchorOffset;
              let anchor = sel.anchorNode;
              if (!anchor.closest) {
                anchor = anchor.parentElement;
              }
              anchor = anchor.closest("[data-offset-key]");
              obsKey = anchor.getAttribute("data-offset-key");

              // set us up to wait for the editor to either update or replace the
              // <span> with that key (the one containing the text to be changed).
              // we will then make sure the cursor is after the pasted text, as if
              // the editor recreates the node, the cursor position is lost
              observer.observe(obsEditor, obsConfig);

              // force the editor to "paste". sending paste or other events will not
              // work, nor using execCommand (adding HTML will screw up the DOM that
              // the editor expects, and adding plain text will make it ignore newlines).
              target.dispatchEvent(
                new InputEvent("beforeinput", {
                  inputType: "insertText",
                  data: text,
                  bubbles: true,
                  cancelable: true,
                })
              );

              // blur the editor to force it to update/flush its state, because otherwise
              // the paste works, but the editor doesn't show it (until it is re-focused).
              obsEditor.blur();
            });
            break;
          }
        }
      },
      true
    );
  })();
}
