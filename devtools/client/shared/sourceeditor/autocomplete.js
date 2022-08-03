/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const AutocompletePopup = require("devtools/client/shared/autocomplete-popup");

loader.lazyRequireGetter(
  this,
  "KeyCodes",
  "devtools/client/shared/keycodes",
  true
);
loader.lazyRequireGetter(
  this,
  "CSSCompleter",
  "devtools/client/shared/sourceeditor/css-autocompleter"
);

const autocompleteMap = new WeakMap();

/**
 * Prepares an editor instance for autocompletion.
 */
function initializeAutoCompletion(ctx, options = {}) {
  const { cm, ed, Editor } = ctx;
  if (autocompleteMap.has(ed)) {
    return;
  }

  const win = ed.container.contentWindow.wrappedJSObject;
  const { CodeMirror } = win;

  let completer = null;
  const autocompleteKey =
    "Ctrl-" + Editor.keyFor("autocompletion", { noaccel: true });
  if (ed.config.mode == Editor.modes.css) {
    completer = new CSSCompleter({
      walker: options.walker,
      cssProperties: options.cssProperties,
    });
  }

  function insertSelectedPopupItem() {
    const autocompleteState = autocompleteMap.get(ed);
    if (!popup || !popup.isOpen || !autocompleteState) {
      return false;
    }

    if (!autocompleteState.suggestionInsertedOnce && popup.selectedItem) {
      autocompleteMap.get(ed).insertingSuggestion = true;
      insertPopupItem(ed, popup.selectedItem);
    }

    popup.once("popup-closed", () => {
      // This event is used in tests.
      ed.emit("popup-hidden");
    });
    popup.hidePopup();
    return true;
  }

  // Give each popup a new name to avoid sharing the elements.

  let popup = new AutocompletePopup(win.parent.document, {
    position: "bottom",
    autoSelect: true,
    onClick: insertSelectedPopupItem,
  });

  const cycle = reverse => {
    if (popup?.isOpen) {
      // eslint-disable-next-line mozilla/no-compare-against-boolean-literals
      cycleSuggestions(ed, reverse == true);
      return null;
    }

    return CodeMirror.Pass;
  };

  let keyMap = {
    Tab: cycle,
    Down: cycle,
    "Shift-Tab": cycle.bind(null, true),
    Up: cycle.bind(null, true),
    Enter: () => {
      const wasHandled = insertSelectedPopupItem();
      return wasHandled ? true : CodeMirror.Pass;
    },
  };

  const autoCompleteCallback = autoComplete.bind(null, ctx);
  const keypressCallback = onEditorKeypress.bind(null, ctx);
  keyMap[autocompleteKey] = autoCompleteCallback;
  cm.addKeyMap(keyMap);

  cm.on("keydown", keypressCallback);
  ed.on("change", autoCompleteCallback);
  ed.on("destroy", destroy);

  function destroy() {
    ed.off("destroy", destroy);
    cm.off("keydown", keypressCallback);
    ed.off("change", autoCompleteCallback);
    cm.removeKeyMap(keyMap);
    popup.destroy();
    keyMap = popup = completer = null;
    autocompleteMap.delete(ed);
  }

  autocompleteMap.set(ed, {
    popup,
    completer,
    keyMap,
    destroy,
    insertingSuggestion: false,
    suggestionInsertedOnce: false,
  });
}

/**
 * Destroy autocompletion on an editor instance.
 */
function destroyAutoCompletion(ctx) {
  const { ed } = ctx;
  if (!autocompleteMap.has(ed)) {
    return;
  }

  const { destroy } = autocompleteMap.get(ed);
  destroy();
}

/**
 * Provides suggestions to autocomplete the current token/word being typed.
 */
function autoComplete({ ed, cm }) {
  const autocompleteOpts = autocompleteMap.get(ed);
  const { completer, popup } = autocompleteOpts;
  if (
    !completer ||
    autocompleteOpts.insertingSuggestion ||
    autocompleteOpts.doNotAutocomplete
  ) {
    autocompleteOpts.insertingSuggestion = false;
    return;
  }
  const cur = ed.getCursor();
  completer
    .complete(cm.getRange({ line: 0, ch: 0 }, cur), cur)
    .then(suggestions => {
      if (
        !suggestions ||
        !suggestions.length ||
        suggestions[0].preLabel == null
      ) {
        autocompleteOpts.suggestionInsertedOnce = false;
        popup.once("popup-closed", () => {
          // This event is used in tests.
          ed.emit("after-suggest");
        });
        popup.hidePopup();
        return;
      }
      // The cursor is at the end of the currently entered part of the token,
      // like "backgr|" but we need to open the popup at the beginning of the
      // character "b". Thus we need to calculate the width of the entered part
      // of the token ("backgr" here).

      const cursorElement = cm.display.cursorDiv.querySelector(
        ".CodeMirror-cursor"
      );
      const left = suggestions[0].preLabel.length * cm.defaultCharWidth();
      popup.hidePopup();
      popup.setItems(suggestions);

      popup.once("popup-opened", () => {
        // This event is used in tests.
        ed.emit("after-suggest");
      });
      popup.openPopup(cursorElement, -1 * left, 0);
      autocompleteOpts.suggestionInsertedOnce = false;
    })
    .catch(console.error);
}

/**
 * Inserts a popup item into the current cursor location
 * in the editor.
 */
function insertPopupItem(ed, popupItem) {
  const { preLabel, text } = popupItem;
  const cur = ed.getCursor();
  const textBeforeCursor = ed.getText(cur.line).substring(0, cur.ch);
  const backwardsTextBeforeCursor = textBeforeCursor
    .split("")
    .reverse()
    .join("");
  const backwardsPreLabel = preLabel
    .split("")
    .reverse()
    .join("");

  // If there is additional text in the preLabel vs the line, then
  // just insert the entire autocomplete text.  An example:
  // if you type 'a' and select '#about' from the autocomplete menu,
  // then the final text needs to the end up as '#about'.
  if (backwardsPreLabel.indexOf(backwardsTextBeforeCursor) === 0) {
    ed.replaceText(text, { line: cur.line, ch: 0 }, cur);
  } else {
    ed.replaceText(text.slice(preLabel.length), cur, cur);
  }
}

/**
 * Cycles through provided suggestions by the popup in a top to bottom manner
 * when `reverse` is not true. Opposite otherwise.
 */
function cycleSuggestions(ed, reverse) {
  const autocompleteOpts = autocompleteMap.get(ed);
  const { popup } = autocompleteOpts;
  const cur = ed.getCursor();
  autocompleteOpts.insertingSuggestion = true;
  if (!autocompleteOpts.suggestionInsertedOnce) {
    autocompleteOpts.suggestionInsertedOnce = true;
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
    if (popup.itemCount == 1) {
      popup.hidePopup();
    }
    insertPopupItem(ed, firstItem);
  } else {
    const fromCur = {
      line: cur.line,
      ch: cur.ch - popup.selectedItem.text.length,
    };
    if (reverse) {
      popup.selectPreviousItem();
    } else {
      popup.selectNextItem();
    }
    ed.replaceText(popup.selectedItem.text, fromCur, cur);
  }
  // This event is used in tests.
  ed.emit("suggestion-entered");
}

/**
 * onkeydown handler for the editor instance to prevent autocompleting on some
 * keypresses.
 */
function onEditorKeypress({ ed, Editor }, cm, event) {
  const autocompleteOpts = autocompleteMap.get(ed);

  // Do not try to autocomplete with multiple selections.
  if (ed.hasMultipleSelections()) {
    autocompleteOpts.doNotAutocomplete = true;
    autocompleteOpts.popup.hidePopup();
    return;
  }

  if (
    (event.ctrlKey || event.metaKey) &&
    event.keyCode == KeyCodes.DOM_VK_SPACE
  ) {
    // When Ctrl/Cmd + Space is pressed, two simultaneous keypresses are emitted
    // first one for just the Ctrl/Cmd and second one for combo. The first one
    // leave the autocompleteOpts.doNotAutocomplete as true, so we have to make
    // it false
    autocompleteOpts.doNotAutocomplete = false;
    return;
  }

  if (event.ctrlKey || event.metaKey || event.altKey) {
    autocompleteOpts.doNotAutocomplete = true;
    autocompleteOpts.popup.hidePopup();
    return;
  }

  switch (event.keyCode) {
    case KeyCodes.DOM_VK_RETURN:
      autocompleteOpts.doNotAutocomplete = true;
      break;
    case KeyCodes.DOM_VK_ESCAPE:
      if (autocompleteOpts.popup.isOpen) {
        // Prevent the Console input to open, but still remove the autocomplete popup.
        autocompleteOpts.doNotAutocomplete = true;
        autocompleteOpts.popup.hidePopup();
        event.preventDefault();
      }
      break;
    case KeyCodes.DOM_VK_LEFT:
    case KeyCodes.DOM_VK_RIGHT:
    case KeyCodes.DOM_VK_HOME:
    case KeyCodes.DOM_VK_END:
      autocompleteOpts.doNotAutocomplete = true;
      autocompleteOpts.popup.hidePopup();
      break;
    case KeyCodes.DOM_VK_BACK_SPACE:
    case KeyCodes.DOM_VK_DELETE:
      if (ed.config.mode == Editor.modes.css) {
        autocompleteOpts.completer.invalidateCache(ed.getCursor().line);
      }
      autocompleteOpts.doNotAutocomplete = true;
      autocompleteOpts.popup.hidePopup();
      break;
    default:
      autocompleteOpts.doNotAutocomplete = false;
  }
}

/**
 * Returns the private popup. This method is used by tests to test the feature.
 */
function getPopup({ ed }) {
  if (autocompleteMap.has(ed)) {
    return autocompleteMap.get(ed).popup;
  }

  return null;
}

/**
 * Returns contextual information about the token covered by the caret if the
 * implementation of completer supports it.
 */
function getInfoAt({ ed }, caret) {
  if (autocompleteMap.has(ed)) {
    const completer = autocompleteMap.get(ed).completer;
    if (completer?.getInfoAt) {
      return completer.getInfoAt(ed.getText(), caret);
    }
  }

  return null;
}

/**
 * Returns whether autocompletion is enabled for this editor.
 * Used for testing
 */
function isAutocompletionEnabled({ ed }) {
  return autocompleteMap.has(ed);
}

// Export functions

module.exports.initializeAutoCompletion = initializeAutoCompletion;
module.exports.destroyAutoCompletion = destroyAutoCompletion;
module.exports.getAutocompletionPopup = getPopup;
module.exports.getInfoAt = getInfoAt;
module.exports.isAutocompletionEnabled = isAutocompletionEnabled;
