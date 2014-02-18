/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const cssAutoCompleter = require("devtools/sourceeditor/css-autocompleter");
const { AutocompletePopup } = require("devtools/shared/autocomplete-popup");

const privates = new WeakMap();

/**
 * Prepares an editor instance for autocompletion, setting up the popup and the
 * CSS completer instance.
 */
function setupAutoCompletion(ctx, walker) {
  let { cm, ed, Editor } = ctx;

  let win = ed.container.contentWindow.wrappedJSObject;

  let completer = null;
  if (ed.config.mode == Editor.modes.css)
    completer = new cssAutoCompleter({walker: walker});

  let popup = new AutocompletePopup(win.parent.document, {
    position: "after_start",
    fixedWidth: true,
    theme: "auto",
    autoSelect: true
  });

  let keyMap = {
    "Tab": cm => {
      if (popup && popup.isOpen) {
        cycleSuggestions(ed);
        return;
      }

      return win.CodeMirror.Pass;
    },
    "Shift-Tab": cm => {
      if (popup && popup.isOpen) {
        cycleSuggestions(ed, true);
        return;
      }

      return win.CodeMirror.Pass;
    },
    "Up": cm => {
      if (popup && popup.isOpen) {
        cycleSuggestions(ed, true);
        return;
      }

      return win.CodeMirror.Pass;
    },
    "Down": cm => {
      if (popup && popup.isOpen) {
        cycleSuggestions(ed);
        return;
      }

      return win.CodeMirror.Pass;
    },
  };
  keyMap[Editor.accel("Space")] = cm => autoComplete(ctx);
  cm.addKeyMap(keyMap);

  cm.on("keydown", (cm, e) => onEditorKeypress(ed, e));
  ed.on("change", () => autoComplete(ctx));
  ed.on("destroy", () => {
    cm.off("keydown", (cm, e) => onEditorKeypress(ed, e));
    ed.off("change", () => autoComplete(ctx));
    popup.destroy();
    popup = null;
    completer = null;
  });

  privates.set(ed, {
    popup: popup,
    completer: completer,
    insertingSuggestion: false,
    suggestionInsertedOnce: false
  });
}

/**
 * Provides suggestions to autocomplete the current token/word being typed.
 */
function autoComplete({ ed, cm }) {
  let private = privates.get(ed);
  let { completer, popup } = private;
  if (!completer || private.insertingSuggestion || private.doNotAutocomplete) {
    private.insertingSuggestion = false;
    return;
  }
  let cur = ed.getCursor();
  completer.complete(cm.getRange({line: 0, ch: 0}, cur), cur)
    .then(suggestions => {
    if (!suggestions || !suggestions.length || !suggestions[0].preLabel) {
      private.suggestionInsertedOnce = false;
      popup.hidePopup();
      ed.emit("after-suggest");
      return;
    }
    // The cursor is at the end of the currently entered part of the token, like
    // "backgr|" but we need to open the popup at the beginning of the character
    // "b". Thus we need to calculate the width of the entered part of the token
    // ("backgr" here). 4 comes from the popup's left padding.
    let left = suggestions[0].preLabel.length * cm.defaultCharWidth() + 4;
    popup.hidePopup();
    popup.setItems(suggestions);
    popup.openPopup(cm.display.cursor, -1 * left, 0);
    private.suggestionInsertedOnce = false;
    // This event is used in tests.
    ed.emit("after-suggest");
  });
}

/**
 * Cycles through provided suggestions by the popup in a top to bottom manner
 * when `reverse` is not true. Opposite otherwise.
 */
function cycleSuggestions(ed, reverse) {
  let private = privates.get(ed);
  let { popup, completer } = private;
  let cur = ed.getCursor();
  private.insertingSuggestion = true;
  if (!private.suggestionInsertedOnce) {
    private.suggestionInsertedOnce = true;
    let firstItem;
    if (reverse) {
      firstItem = popup.getItemAtIndex(popup.itemCount - 1);
      popup.selectPreviousItem();
    } else {
      firstItem = popup.getItemAtIndex(0);
      if (firstItem.label == firstItem.preLabel && popup.itemCount > 1) {
        firstItem = popup.getItemAtIndex(1);
        popup.selectNextItem();
      }
    }
    if (popup.itemCount == 1)
      popup.hidePopup();
    ed.replaceText(firstItem.label.slice(firstItem.preLabel.length), cur, cur);
  } else {
    let fromCur = {
      line: cur.line,
      ch  : cur.ch - popup.selectedItem.label.length
    };
    if (reverse)
      popup.selectPreviousItem();
    else
      popup.selectNextItem();
    ed.replaceText(popup.selectedItem.label, fromCur, cur);
  }
  // This event is used in tests.
  ed.emit("suggestion-entered");
}

/**
 * onkeydown handler for the editor instance to prevent autocompleting on some
 * keypresses.
 */
function onEditorKeypress(ed, event) {
  let private = privates.get(ed);
  switch (event.keyCode) {
    case event.DOM_VK_ESCAPE:
      if (private.popup.isOpen)
        event.preventDefault();
    case event.DOM_VK_LEFT:
    case event.DOM_VK_RIGHT:
    case event.DOM_VK_HOME:
    case event.DOM_VK_END:
    case event.DOM_VK_BACK_SPACE:
    case event.DOM_VK_DELETE:
    case event.DOM_VK_ENTER:
    case event.DOM_VK_RETURN:
      private.doNotAutocomplete = true;
      private.popup.hidePopup();
      break;

    default:
      private.doNotAutocomplete = false;
  }
}

/**
 * Returns the private popup. This method is used by tests to test the feature.
 */
function getPopup({ ed }) {
  return privates.get(ed).popup;
}
// Export functions

module.exports.setupAutoCompletion = setupAutoCompletion;
module.exports.getAutocompletionPopup = getPopup;
