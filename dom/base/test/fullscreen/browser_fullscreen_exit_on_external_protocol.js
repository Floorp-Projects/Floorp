/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

SimpleTest.requestCompleteLog();

requestLongerTimeout(2);

// Import helpers
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/base/test/fullscreen/fullscreen_helpers.js",
  this
);

add_setup(async function () {
  await pushPrefs(
    ["full-screen-api.transition-duration.enter", "0 0"],
    ["full-screen-api.transition-duration.leave", "0 0"],
    ["full-screen-api.allow-trusted-requests-only", false]
  );
});

const { HandlerServiceTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/HandlerServiceTestUtils.sys.mjs"
);

const gHandlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].getService(
  Ci.nsIHandlerService
);

const CONTENT = `data:text/html,
    <!DOCTYPE html>
    <html>
        <body>
            <button>
                <a href="mailto:test@example.com"></a>
            </button>
        </body>
    </html>
`;

// This test tends to trigger a race in the fullscreen time telemetry,
// where the fullscreen enter and fullscreen exit events (which use the
// same histogram ID) overlap. That causes TelemetryStopwatch to log an
// error.
SimpleTest.ignoreAllUncaughtExceptions(true);

function setupMailHandler() {
  let mailHandlerInfo = HandlerServiceTestUtils.getHandlerInfo("mailto");
  let gOldMailHandlers = [];

  // Remove extant web handlers because they have icons that
  // we fetch from the web, which isn't allowed in tests.
  let handlers = mailHandlerInfo.possibleApplicationHandlers;
  for (let i = handlers.Count() - 1; i >= 0; i--) {
    try {
      let handler = handlers.queryElementAt(i, Ci.nsIWebHandlerApp);
      gOldMailHandlers.push(handler);
      // If we get here, this is a web handler app. Remove it:
      handlers.removeElementAt(i);
    } catch (ex) {}
  }

  let previousHandling = mailHandlerInfo.alwaysAskBeforeHandling;
  mailHandlerInfo.alwaysAskBeforeHandling = true;

  // Create a dummy web mail handler so we always know the mailto: protocol.
  // Without this, the test fails on VMs without a default mailto: handler,
  // because no dialog is ever shown, as we ignore subframe navigations to
  // protocols that cannot be handled.
  let dummy = Cc["@mozilla.org/uriloader/web-handler-app;1"].createInstance(
    Ci.nsIWebHandlerApp
  );
  dummy.name = "Handler 1";
  dummy.uriTemplate = "https://example.com/first/%s";
  mailHandlerInfo.possibleApplicationHandlers.appendElement(dummy);

  gHandlerSvc.store(mailHandlerInfo);
  registerCleanupFunction(() => {
    // Re-add the original protocol handlers:
    let mailHandlers = mailHandlerInfo.possibleApplicationHandlers;
    for (let i = handlers.Count() - 1; i >= 0; i--) {
      try {
        // See if this is a web handler. If it is, it'll throw, otherwise,
        // we will remove it.
        mailHandlers.queryElementAt(i, Ci.nsIWebHandlerApp);
        mailHandlers.removeElementAt(i);
      } catch (ex) {}
    }
    for (let h of gOldMailHandlers) {
      mailHandlers.appendElement(h);
    }
    mailHandlerInfo.alwaysAskBeforeHandling = previousHandling;
    gHandlerSvc.store(mailHandlerInfo);
  });
}

add_task(setupMailHandler);

// Fullscreen is canceled during fullscreen transition
add_task(async function OpenExternalProtocolOnPendingLaterFullscreen() {
  for (const useClick of [true, false]) {
    await BrowserTestUtils.withNewTab(CONTENT, async browser => {
      const leavelFullscreen = waitForFullscreenState(document, false, true);
      await SpecialPowers.spawn(
        browser,
        [useClick],
        async function (shouldClick) {
          const button = content.document.querySelector("button");

          const clickDone = new Promise(r => {
            button.addEventListener(
              "click",
              function () {
                content.document.documentElement.requestFullscreen();
                // When anchor.click() is called, the fullscreen request
                // is probably still pending.
                content.setTimeout(() => {
                  if (shouldClick) {
                    content.document.querySelector("a").click();
                  } else {
                    content.document.location = "mailto:test@example.com";
                  }
                  r();
                }, 0);
              },
              { once: true }
            );
          });
          button.click();
          await clickDone;
        }
      );

      await leavelFullscreen;
      ok(true, "Fullscreen should be exited");
    });
  }
});

// Fullscreen is canceled immediately.
add_task(async function OpenExternalProtocolOnPendingFullscreen() {
  for (const useClick of [true, false]) {
    await BrowserTestUtils.withNewTab(CONTENT, async browser => {
      await SpecialPowers.spawn(
        browser,
        [useClick],
        async function (shouldClick) {
          const button = content.document.querySelector("button");

          const clickDone = new Promise(r => {
            button.addEventListener(
              "click",
              function () {
                content.document.documentElement
                  .requestFullscreen()
                  .then(() => {
                    ok(false, "Don't enter fullscreen");
                  })
                  .catch(() => {
                    ok(true, "Cancel entering fullscreen");
                    r();
                  });
                // When anchor.click() is called, the fullscreen request
                // is probably still pending.
                if (shouldClick) {
                  content.document.querySelector("a").click();
                } else {
                  content.document.location = "mailto:test@example.com";
                }
              },
              { once: true }
            );
          });
          button.click();
          await clickDone;
        }
      );

      ok(true, "Fullscreen should be exited");
    });
  }
});

add_task(async function OpenExternalProtocolOnFullscreen() {
  for (const useClick of [true, false]) {
    await BrowserTestUtils.withNewTab(CONTENT, async browser => {
      const leavelFullscreen = waitForFullscreenState(document, false, true);
      await SpecialPowers.spawn(
        browser,
        [useClick],
        async function (shouldClick) {
          let button = content.document.querySelector("button");
          button.addEventListener("click", function () {
            content.document.documentElement.requestFullscreen();
          });
          button.click();

          await new Promise(r => {
            content.document.addEventListener("fullscreenchange", r);
          });

          if (shouldClick) {
            content.document.querySelector("a").click();
          } else {
            content.document.location = "mailto:test@example.com";
          }
        }
      );

      await leavelFullscreen;
      ok(true, "Fullscreen should be exited");
    });
  }
});
