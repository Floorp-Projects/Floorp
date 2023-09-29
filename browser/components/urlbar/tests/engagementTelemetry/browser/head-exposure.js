/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doExposureTest({
  prefs,
  query,
  trigger,
  assert,
  select = defaultSelect,
}) {
  const cleanupQuickSuggest = await ensureQuickSuggestInit();
  await SpecialPowers.pushPrefEnv({
    set: prefs,
  });

  await doTest(async () => {
    await openPopup(query);
    await select(query);

    await trigger();
    await assert();
  });

  await SpecialPowers.popPrefEnv();
  await cleanupQuickSuggest();
}

async function defaultSelect(query) {
  await selectRowByURL(`https://example.com/${query}`);
}

async function getResultByType(provider) {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    const detail = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    const telemetryType = UrlbarUtils.searchEngagementTelemetryType(
      detail.result
    );
    if (telemetryType === provider) {
      return detail.result;
    }
  }
  return null;
}
