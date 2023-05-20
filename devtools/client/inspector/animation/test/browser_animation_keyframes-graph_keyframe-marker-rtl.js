/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  Services.scriptloader.loadSubScript(
    CHROME_URL_ROOT + "keyframes-graph_keyframe-marker_head.js",
    this
  );
  await pushPref("intl.l10n.pseudo", "bidi");
  // eslint-disable-next-line no-undef
  await testKeyframesGraphKeyframesMarker();
});
