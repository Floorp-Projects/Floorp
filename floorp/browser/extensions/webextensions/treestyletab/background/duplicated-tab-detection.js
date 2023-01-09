/*
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
'use strict';

import {
  configs,
} from '/common/common.js';
import * as Constants from '/common/constants.js';

import Tab from '/common/Tab.js';

async function doTest() {
  const win = await browser.windows.getLastFocused({ populate: true });
  const tab = win.tabs.find(tab => tab.active);

  const maxTry = 10;
  const promises = [];
  const duplicatedTabIds = [];
  for (let i = 0; i < maxTry; i++) {
    promises.push(browser.tabs.duplicate(tab.id).then(async duplicated => {
      await Tab.waitUntilTracked(duplicated.id);
      const tracked = Tab.get(duplicated.id);
      const uniqueId = await tracked.$TST.promisedUniqueId;
      duplicatedTabIds.push(duplicated.id);
      return uniqueId && uniqueId.duplicated;
    }));
  }
  const successCount = (await Promise.all(promises)).filter(result => !!result).length;
  await browser.tabs.remove(duplicatedTabIds);
  return successCount / maxTry;
}

async function autoDetectSuitableDelay() {
  configs.delayForDuplicatedTabDetection = 0;
  let successRate = await doTest();
  if (successRate == 1)
    return;

  configs.delayForDuplicatedTabDetection = 10;
  while (successRate < 1) {
    configs.delayForDuplicatedTabDetection = Math.round(configs.delayForDuplicatedTabDetection * (1 / Math.max(successRate, 0.5)));
    successRate = await doTest();
  }
}

browser.runtime.onMessage.addListener((message, _sender) => {
  if (!message || !message.type)
    return;

  switch (message.type) {
    case Constants.kCOMMAND_TEST_DUPLICATED_TAB_DETECTION:
      return doTest();

    case Constants.kCOMMAND_AUTODETECT_DUPLICATED_TAB_DETECTION_DELAY:
      autoDetectSuitableDelay();
      return;
  }
});
