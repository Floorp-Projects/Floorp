/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

addAccessibleTask(
  `
<script>
  // We can't include the html element in snippets, so set lang on it here.
  document.documentElement.lang = "en";
</script>
<div id="inheritEn"></div>
<div id="de" lang="de">
  <div id="inheritDe"></div>
  <div id="fr" lang="fr"></div>
</div>
  `,
  async function (browser, docAcc) {
    is(docAcc.language, "en", "Document language correct");
    const inheritEn = findAccessibleChildByID(docAcc, "inheritEn");
    is(inheritEn.language, "en", "inheritEn language correct");
    const de = findAccessibleChildByID(docAcc, "de");
    is(de.language, "de", "de language correct");
    const fr = findAccessibleChildByID(docAcc, "fr");
    is(fr.language, "fr", "fr language correct");
  },
  { chrome: true, topLevel: true, remoteIframe: true }
);
