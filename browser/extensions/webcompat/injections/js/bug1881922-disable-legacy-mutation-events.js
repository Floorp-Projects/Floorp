/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bugs 1881922, 1901780 - Disable legacy DOM Mutation Events to prevent performance issues.
 */

/* globals exportFunction */

let bug = "1881922";
if (location.origin.includes("vanbreda")) {
  bug = "1901780";
}
console.info(
  `DOM Mutation Events have been disabled to prevent performance issues. See https://bugzilla.mozilla.org/show_bug.cgi?id=${bug} for details.`
);

(function disableMutationEvents() {
  const whichEvents = [
    "domattrmodified",
    "domcharacterdatamodified",
    "domnodeinserted",
    "domnodeinsertedintodocument",
    "domnoderemoved",
    "domnoderemovedfromdocument",
    "domsubtreemodified",
  ];

  const { prototype } = window.wrappedJSObject.EventTarget;
  const { addEventListener } = prototype;
  Object.defineProperty(prototype, "addEventListener", {
    value: exportFunction(function (_type, b, c, d) {
      const type = _type?.toLowerCase();
      if (whichEvents.includes(type)) {
        return undefined;
      }
      return addEventListener.call(this, type, b, c, d);
    }, window),
  });
})();
