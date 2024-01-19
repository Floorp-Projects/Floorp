/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

function _isInvalidTarget(target) {
  if (!target || !target.innerText) {
    return true;
  }

  const tokenText = target.innerText.trim();

  // exclude syntax where the expression would be a syntax error
  const invalidToken =
    tokenText === "" || tokenText.match(/^[(){}\|&%,.;=<>\+-/\*\s](?=)/);
  if (invalidToken) {
    return true;
  }

  // exclude tokens for which it does not make sense to show a preview:
  // - literal
  // - primitives
  // - operators
  // - tags
  const INVALID_TARGET_CLASSES = [
    "cm-atom",
    "cm-number",
    "cm-operator",
    "cm-string",
    "cm-tag",
    // also exclude editor element (defined in Editor component)
    "editor-mount",
  ];
  if (
    target.className === "" ||
    INVALID_TARGET_CLASSES.some(cls => target.classList.contains(cls))
  ) {
    return true;
  }

  // We need to exclude keywords, but since codeMirror tags "this" as a keyword, we need
  // to check the tokenText as well.
  // This seems to be the only case that we want to exclude (see devtools/client/shared/sourceeditor/codemirror/mode/javascript/javascript.js#24-41)
  if (target.classList.contains("cm-keyword") && tokenText !== "this") {
    return true;
  }

  // exclude codemirror elements that are not tokens
  if (
    // exclude inline preview
    target.closest(".CodeMirror-widget") ||
    // exclude in-line "empty" space, as well as the gutter
    target.matches(".CodeMirror-line, .CodeMirror-gutter-elt") ||
    target.getBoundingClientRect().top == 0
  ) {
    return true;
  }

  // exclude popup
  if (target.closest(".popover")) {
    return true;
  }

  return false;
}

function _dispatch(codeMirror, eventName, data) {
  codeMirror.constructor.signal(codeMirror, eventName, data);
}

function _invalidLeaveTarget(target) {
  if (!target || target.closest(".popover")) {
    return true;
  }

  return false;
}

/**
 * Wraps the codemirror mouse events  to generate token events
 * @param {*} codeMirror
 * @returns
 */
export function onMouseOver(codeMirror) {
  let prevTokenPos = null;

  function onMouseLeave(event) {
    if (_invalidLeaveTarget(event.relatedTarget)) {
      addMouseLeave(event.target);
      return;
    }

    prevTokenPos = null;
    _dispatch(codeMirror, "tokenleave", event);
  }

  function addMouseLeave(target) {
    target.addEventListener("mouseleave", onMouseLeave, {
      capture: true,
      once: true,
    });
  }

  return enterEvent => {
    const { target } = enterEvent;

    if (_isInvalidTarget(target)) {
      return;
    }

    const tokenPos = getTokenLocation(codeMirror, target);

    if (
      prevTokenPos?.line !== tokenPos?.line ||
      prevTokenPos?.column !== tokenPos?.column
    ) {
      addMouseLeave(target);

      _dispatch(codeMirror, "tokenenter", {
        event: enterEvent,
        target,
        tokenPos,
      });
      prevTokenPos = tokenPos;
    }
  };
}

/**
 * Gets the end position of a token at a specific line/column
 *
 * @param {*} codeMirror
 * @param {Number} line
 * @param {Number} column
 * @returns {Number}
 */
export function getTokenEnd(codeMirror, line, column) {
  const token = codeMirror.getTokenAt({
    line,
    ch: column + 1,
  });
  const tokenString = token.string;

  return tokenString === "{" || tokenString === "[" ? null : token.end;
}

/**
 * Given the dom element related to the token, this gets its line and column.
 *
 * @param {*} codeMirror
 * @param {*} tokenEl
 * @returns {Object} An object of the form { line, column }
 */
export function getTokenLocation(codeMirror, tokenEl) {
  // Get the quad (and not the bounding rect), as the span could wrap on multiple lines
  // and the middle of the bounding rect may not be over the token:
  // +───────────────────────+
  // │      myLongVariableNa│
  // │me         +          │
  // +───────────────────────+
  const { p1, p2, p3 } = tokenEl.getBoxQuads()[0];
  const left = p1.x + (p2.x - p1.x) / 2;
  const top = p1.y + (p3.y - p1.y) / 2;
  const { line, ch } = codeMirror.coordsChar(
    {
      left,
      top,
    },
    // Use the "window" context where the coordinates are relative to the top-left corner
    // of the currently visible (scrolled) window.
    // This enables codemirror also correctly handle wrappped lines in the editor.
    "window"
  );

  return {
    line: line + 1,
    column: ch,
  };
}
