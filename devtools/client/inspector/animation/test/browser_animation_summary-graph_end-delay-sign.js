/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  Services.scriptloader.loadSubScript(
    CHROME_URL_ROOT + "summary-graph_end-delay-sign_head.js", this);
  // eslint-disable-next-line no-undef
  await testSummaryGraphEndDelaySign();
});
