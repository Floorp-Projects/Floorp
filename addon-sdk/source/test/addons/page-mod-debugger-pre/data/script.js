/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

function runDebuggerStatement () {
  window.document.body.setAttribute('style', 'background-color: red');
  debugger;
  window.document.body.setAttribute('style', 'background-color: green');
}

exportFunction(
  runDebuggerStatement,
  document.defaultView,
  { defineAs: "runDebuggerStatement" }
);
