/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from summary-graph_end-delay-sign_head.js */

add_task(async function() {
  Services.scriptloader.loadSubScript(
    CHROME_URL_ROOT + "summary-graph_end-delay-sign_head.js", this);
  await pushPref("intl.uidirection", 1);
  await testSummaryGraphEndDelaySign();
});
