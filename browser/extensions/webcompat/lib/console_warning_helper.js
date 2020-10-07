/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals module */

const ConsoleWarningEyeCatch = "Oh no!";
const ConsoleWarningDetails = `This web site has a web compatibility issue in Firefox. If you see this message and are responsible for working on $DOMAIN$, please investigate. More details about this issue and how to disable our workaround can be found on our about:compat page.`;

function promiseConsoleWarningScript(domain) {
  const details = ConsoleWarningDetails.replace("$DOMAIN$", domain);
  return Promise.resolve({
    code: `if (!window.alreadyWarned) {
             window.alreadyWarned = true;
             console.warn("%c${ConsoleWarningEyeCatch}",
               "font-size:50px; font-weight:bold; color:red; -webkit-text-stroke:1px black",
               ${JSON.stringify("\n" + details)});
           }`,
  });
}

module.exports = promiseConsoleWarningScript;
