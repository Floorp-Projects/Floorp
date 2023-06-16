/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  await withAboutProfiling(async document => {
    is(document.dir, "ltr", "About profiling has the expected direction ltr");
    is(
      document.documentElement.getAttribute("lang"),
      "en-US",
      "About profiling has the expected lang"
    );
  });
});

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["intl.l10n.pseudo", "bidi"]],
  });

  await withAboutProfiling(async document => {
    is(document.dir, "rtl", "About profiling has the expected direction rtl");
    is(
      document.documentElement.getAttribute("lang"),
      "en-US",
      "About profiling has the expected lang"
    );
  });
});
