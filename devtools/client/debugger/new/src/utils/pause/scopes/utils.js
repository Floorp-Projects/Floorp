"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getFramePopVariables = getFramePopVariables;
exports.getThisVariable = getThisVariable;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getFramePopVariables(why, path) {
  const vars = [];

  if (why && why.frameFinished) {
    const frameFinished = why.frameFinished; // Always display a `throw` property if present, even if it is falsy.

    if (Object.prototype.hasOwnProperty.call(frameFinished, "throw")) {
      vars.push({
        name: "<exception>",
        path: `${path}/<exception>`,
        contents: {
          value: frameFinished.throw
        }
      });
    }

    if (Object.prototype.hasOwnProperty.call(frameFinished, "return")) {
      const returned = frameFinished.return; // Do not display undefined. Do display falsy values like 0 and false. The
      // protocol grip for undefined is a JSON object: { type: "undefined" }.

      if (typeof returned !== "object" || returned.type !== "undefined") {
        vars.push({
          name: "<return>",
          path: `${path}/<return>`,
          contents: {
            value: returned
          }
        });
      }
    }
  }

  return vars;
}

function getThisVariable(this_, path) {
  if (!this_) {
    return null;
  }

  return {
    name: "<this>",
    path: `${path}/<this>`,
    contents: {
      value: this_
    }
  };
}