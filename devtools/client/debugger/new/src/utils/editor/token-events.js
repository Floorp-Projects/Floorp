"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.onMouseOver = onMouseOver;

var _ = require("./index");

var _lodash = require("devtools/client/shared/vendor/lodash");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function isInvalidTarget(target) {
  if (!target || !target.innerText) {
    return true;
  }

  const tokenText = target.innerText.trim();
  const cursorPos = target.getBoundingClientRect(); // exclude literal tokens where it does not make sense to show a preview

  const invalidType = ["cm-atom", ""].includes(target.className); // exclude syntax where the expression would be a syntax error

  const invalidToken = tokenText === "" || tokenText.match(/^[(){}\|&%,.;=<>\+-/\*\s](?=)/); // exclude codemirror elements that are not tokens

  const invalidTarget = target.parentElement && !target.parentElement.closest(".CodeMirror-line") || cursorPos.top == 0;
  const invalidClasses = ["editor-mount"];

  if (invalidClasses.some(className => target.classList.contains(className))) {
    return true;
  }

  if (target.closest(".popover")) {
    return true;
  }

  return invalidTarget || invalidToken || invalidType;
}

function dispatch(codeMirror, eventName, data) {
  codeMirror.constructor.signal(codeMirror, eventName, data);
}

function invalidLeaveTarget(target) {
  if (!target || target.closest(".popover")) {
    return true;
  }

  return false;
}

function onMouseOver(codeMirror) {
  let prevTokenPos = null;

  function onMouseLeave(event) {
    if (invalidLeaveTarget(event.relatedTarget)) {
      return addMouseLeave(event.target);
    }

    prevTokenPos = null;
    dispatch(codeMirror, "tokenleave", event);
  }

  function addMouseLeave(target) {
    target.addEventListener("mouseleave", onMouseLeave, {
      capture: true,
      once: true
    });
  }

  return enterEvent => {
    const {
      target
    } = enterEvent;

    if (isInvalidTarget(target)) {
      return;
    }

    const tokenPos = (0, _.getTokenLocation)(codeMirror, target);

    if (!(0, _lodash.isEqual)(prevTokenPos, tokenPos)) {
      addMouseLeave(target);
      dispatch(codeMirror, "tokenenter", {
        event: enterEvent,
        target,
        tokenPos
      });
      prevTokenPos = tokenPos;
    }
  };
}