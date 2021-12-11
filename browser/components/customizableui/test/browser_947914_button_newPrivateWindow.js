/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  info("Check private browsing button existence and functionality");
  CustomizableUI.addWidgetToArea(
    "privatebrowsing-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );
  registerCleanupFunction(() => CustomizableUI.reset());

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  info("Menu panel was opened");

  let windowWasHandled = false;
  let privateWindow = null;

  let observerWindowOpened = {
    observe(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        privateWindow = aSubject;
        privateWindow.addEventListener(
          "load",
          function() {
            is(
              privateWindow.location.href,
              AppConstants.BROWSER_CHROME_URL,
              "A new browser window was opened"
            );
            ok(
              PrivateBrowsingUtils.isWindowPrivate(privateWindow),
              "Window is private"
            );
            windowWasHandled = true;
          },
          { once: true }
        );
      }
    },
  };

  Services.ww.registerNotification(observerWindowOpened);

  let privateBrowsingButton = document.getElementById("privatebrowsing-button");
  ok(privateBrowsingButton, "Private browsing button exists in Panel Menu");
  privateBrowsingButton.click();

  try {
    await TestUtils.waitForCondition(() => windowWasHandled);
    await promiseWindowClosed(privateWindow);
    info("The new private window was closed");
  } catch (e) {
    ok(false, "The new private browser window was not properly handled");
  } finally {
    Services.ww.unregisterNotification(observerWindowOpened);
  }
});
