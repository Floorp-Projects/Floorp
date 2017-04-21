/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

console.log(document, document.body);
console.assert(false, "Failing mozscreenshots assertion");

console.group("Grouped Message");
console.log("group message 1");
console.groupEnd();

console.count("counter");
console.count("counter");
