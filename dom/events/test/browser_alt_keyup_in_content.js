"use strict";

add_task(async function runTests() {
  const menubar = document.getElementById("toolbar-menubar");
  const autohide = menubar.getAttribute("autohide");
  // This test requires that the window is active because of the limitation of
  // menubar.  Therefore, we should abort if the window becomes inactive during
  // the tests.
  let runningTests = true;
  function onWindowActive(aEvent) {
    // Don't warn after timed out.
    if (runningTests && aEvent.target === window) {
      info(
        "WARNING: This window shouldn't have been inactivated during tests, but received an activated event!"
      );
    }
  }
  function onWindowInactive(aEvent) {
    // Don't warn after timed out.
    if (runningTests && aEvent.target === window) {
      info(
        "WARNING: This window should be active during tests, but inactivated!"
      );
      window.focus();
    }
  }
  let menubarActivated = false;
  function onMenubarActive() {
    menubarActivated = true;
  }
  // In this test, menu popups shouldn't be open, but this helps avoiding
  // intermittent failure after inactivating the menubar.
  let popupEvents = 0;
  function getPopupInfo(aPopupEventTarget) {
    return `<${
      aPopupEventTarget.nodeName
    }${aPopupEventTarget.getAttribute("id") !== null ? ` id="${aPopupEventTarget.getAttribute("id")}"` : ""}>`;
  }
  function onPopupShown(aEvent) {
    // Don't warn after timed out.
    if (!runningTests) {
      return;
    }
    popupEvents++;
    info(
      `A popup (${getPopupInfo(
        aEvent.target
      )}) is shown (visible popups: ${popupEvents})`
    );
  }
  function onPopupHidden(aEvent) {
    // Don't warn after timed out.
    if (!runningTests) {
      return;
    }
    if (popupEvents === 0) {
      info(
        `WARNING: There are some unexpected popups which may be not cleaned up by the previous test (${getPopupInfo(
          aEvent.target
        )})`
      );
      return;
    }
    popupEvents--;
    info(
      `A popup (${getPopupInfo(
        aEvent.target
      )}) is hidden (visible popups: ${popupEvents})`
    );
  }
  try {
    Services.prefs.setBoolPref("ui.key.menuAccessKeyFocuses", true);
    // If this fails, you need to replace "KEY_Alt" with a variable whose
    // value is considered from the pref.
    is(
      Services.prefs.getIntPref("ui.key.menuAccessKey"),
      18,
      "This test assumes that Alt key activates the menubar"
    );
    window.addEventListener("activate", onWindowActive);
    window.addEventListener("deactivate", onWindowInactive);
    window.addEventListener("popupshown", onPopupShown);
    window.addEventListener("popuphidden", onPopupHidden);
    menubar.addEventListener("DOMMenuBarActive", onMenubarActive);
    async function doTest(aTest) {
      await new Promise(resolve => {
        if (Services.focus.activeWindow === window) {
          resolve();
          return;
        }
        info(
          `${aTest.description}: The testing window is inactive, trying to activate it...`
        );
        Services.focus.focusedWindow = window;
        TestUtils.waitForCondition(() => {
          if (Services.focus.activeWindow === window) {
            resolve();
            return true;
          }
          Services.focus.focusedWindow = window;
          return false;
        }, `${aTest.description}: Waiting the window is activated`);
      });
      let startTime = performance.now();
      info(`Start to test: ${aTest.description}...`);

      async function ensureMenubarInactive() {
        if (!menubar.querySelector("[_moz-menuactive=true]")) {
          return;
        }
        info(`${aTest.description}: Inactivating the menubar...`);
        let waitForMenuBarInactive = BrowserTestUtils.waitForEvent(
          menubar,
          "DOMMenuBarInactive"
        );
        EventUtils.synthesizeKey("KEY_Escape", {}, window);
        await waitForMenuBarInactive;
        await TestUtils.waitForCondition(() => {
          return popupEvents === 0;
        }, `${aTest.description}: Waiting for closing all popups`);
      }

      try {
        await BrowserTestUtils.withNewTab(
          {
            gBrowser,
            url: aTest.url,
          },
          async browser => {
            info(`${aTest.description}: Waiting browser getting focus...`);
            await SimpleTest.promiseFocus(browser);
            await ensureMenubarInactive();
            menubarActivated = false;

            let keyupEventFiredInContent = false;
            BrowserTestUtils.addContentEventListener(
              browser,
              "keyup",
              () => {
                keyupEventFiredInContent = true;
              },
              { capture: true },
              event => {
                return event.key === "Alt";
              }
            );

            // For making sure adding the above content event listener and
            // it'll get `keyup` event, let's run `SpecialPowers.spawn` and
            // wait for focus in the content process.
            info(
              `${aTest.description}: Waiting content process getting focus...`
            );
            await SpecialPowers.spawn(
              browser,
              [aTest.description],
              async aTestDescription => {
                await ContentTaskUtils.waitForCondition(() => {
                  if (
                    content.browsingContext.isActive &&
                    content.document.hasFocus()
                  ) {
                    return true;
                  }
                  content.window.focus();
                  return false;
                }, `${aTestDescription}: Waiting for content gets focus in content process`);
              }
            );

            let waitForAllKeyUpEventsInChrome = new Promise(resolve => {
              // Wait 2 `keyup` events in the main process.  First one is
              // synthesized one.  The other is replay event from content.
              let firstKeyUpEvent;
              window.addEventListener(
                "keyup",
                function onKeyUpInChrome(event) {
                  if (!firstKeyUpEvent) {
                    firstKeyUpEvent = event;
                    return;
                  }
                  window.removeEventListener("keyup", onKeyUpInChrome, {
                    capture: true,
                  });
                  resolve();
                },
                { capture: true }
              );
            });

            let menubarActivatedPromise;
            if (aTest.expectMenubarActive) {
              menubarActivatedPromise = BrowserTestUtils.waitForEvent(
                menubar,
                "DOMMenuBarActive"
              );
            }

            EventUtils.synthesizeKey("KEY_Alt", {}, window);
            info(
              `${aTest.description}: Waiting keyup events of Alt in chrome...`
            );
            await waitForAllKeyUpEventsInChrome;
            info(`${aTest.description}: Waiting keyup event in content...`);
            try {
              await TestUtils.waitForCondition(() => {
                return keyupEventFiredInContent;
              }, `${aTest.description}: Waiting for content gets focus in chrome process`);
            } catch (ex) {
              ok(
                false,
                `${aTest.description}: Failed to synthesize Alt key press in the content process`
              );
              return;
            }

            if (aTest.expectMenubarActive) {
              await menubarActivatedPromise;
              ok(
                menubarActivated,
                `${aTest.description}: Menubar should've been activated by the synthesized Alt key press`
              );
            } else {
              // Wait some ticks to verify not receiving "DOMMenuBarActive" event.
              // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
              await new Promise(resolve => setTimeout(resolve, 500));
              ok(
                !menubarActivated,
                `${aTest.description}: Menubar should not have been activated by the synthesized Alt key press`
              );
            }
          }
        );
      } catch (ex) {
        ok(
          false,
          `${aTest.description}: Thrown an exception: ${ex.toString()}`
        );
      } finally {
        await ensureMenubarInactive();
        info(`End testing: ${aTest.description}`);
        ChromeUtils.addProfilerMarker(
          "browser-test",
          { startTime, category: "Test" },
          aTest.description
        );
      }
    }

    // Testcases for users who use collapsible menubar (by default)
    menubar.setAttribute("autohide", "true");
    await doTest({
      description: "Testing menubar is shown by Alt keyup",
      url: "data:text/html;charset=utf-8,<p>static page</p>",
      expectMenubarActive: true,
    });
    await doTest({
      description:
        "Testing menubar is shown by Alt keyup when an <input> has focus",
      url:
        "data:text/html;charset=utf-8,<input>" +
        '<script>document.querySelector("input").focus()</script>',
      expectMenubarActive: true,
    });
    await doTest({
      description:
        "Testing menubar is shown by Alt keyup when an editing host has focus",
      url:
        "data:text/html;charset=utf-8,<p contenteditable></p>" +
        '<script>document.querySelector("p[contenteditable]").focus()</script>',
      expectMenubarActive: true,
    });
    await doTest({
      description:
        "Testing menubar won't be shown by Alt keyup due to suppressed by the page",
      url:
        "data:text/html;charset=utf-8,<p>dynamic page</p>" +
        '<script>window.addEventListener("keyup", event => { event.preventDefault(); })</script>',
      expectMenubarActive: false,
    });

    // Testcases for users who always show the menubar.
    menubar.setAttribute("autohide", "false");
    await doTest({
      description: "Testing menubar is activated by Alt keyup",
      url: "data:text/html;charset=utf-8,<p>static page</p>",
      expectMenubarActive: true,
    });
    await doTest({
      description:
        "Testing menubar is activated by Alt keyup when an <input> has focus",
      url:
        "data:text/html;charset=utf-8,<input>" +
        '<script>document.querySelector("input").focus()</script>',
      expectMenubarActive: true,
    });
    await doTest({
      description:
        "Testing menubar is activated by Alt keyup when an editing host has focus",
      url:
        "data:text/html;charset=utf-8,<p contenteditable></p>" +
        '<script>document.querySelector("p[contenteditable]").focus()</script>',
      expectMenubarActive: true,
    });
    await doTest({
      description:
        "Testing menubar won't be activated by Alt keyup due to suppressed by the page",
      url:
        "data:text/html;charset=utf-8,<p>dynamic page</p>" +
        '<script>window.addEventListener("keyup", event => { event.preventDefault(); })</script>',
      expectMenubarActive: false,
    });
    runningTests = false;
  } catch (ex) {
    ok(
      false,
      `Aborting this test due to unexpected the exception (${ex.toString()})`
    );
    runningTests = false;
  } finally {
    if (autohide !== null) {
      menubar.setAttribute("autohide", autohide);
    } else {
      menubar.removeAttribute("autohide");
    }
    Services.prefs.clearUserPref("ui.key.menuAccessKeyFocuses");
    menubar.removeEventListener("DOMMenuBarActive", onMenubarActive);
    window.removeEventListener("activate", onWindowActive);
    window.removeEventListener("deactivate", onWindowInactive);
    window.removeEventListener("popupshown", onPopupShown);
    window.removeEventListener("popuphidden", onPopupHidden);
  }
});
