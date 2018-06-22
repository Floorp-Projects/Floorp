/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from current-time-scrubber_head.js */

add_task(async function() {
  Services.scriptloader.loadSubScript(
    CHROME_URL_ROOT + "current-time-scrubber_head.js", this);
  await testCurrentTimeScrubber();
});
