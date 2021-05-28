const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
function waitForFullScreenState(browser, state) {
  return new Promise(resolve => {
    let eventReceived = false;

    let observe = (subject, topic, data) => {
      if (!eventReceived) {
        return;
      }
      Services.obs.removeObserver(observe, "fullscreen-painted");
      resolve();
    };
    Services.obs.addObserver(observe, "fullscreen-painted");

    browser.ownerGlobal.addEventListener(
      `MozDOMFullscreen:${state ? "Entered" : "Exited"}`,
      () => {
        eventReceived = true;
      },
      { once: true }
    );
  });
}

/**
 * Spawns content task in browser to enter / leave fullscreen
 * @param browser - Browser to use for JS fullscreen requests
 * @param {Boolean} fullscreenState - true to enter fullscreen, false to leave
 * @returns {Promise} - Resolves once fullscreen change is applied
 */
async function changeFullscreen(browser, fullScreenState) {
  await new Promise(resolve =>
    SimpleTest.waitForFocus(resolve, browser.ownerGlobal)
  );
  let fullScreenChange = waitForFullScreenState(browser, fullScreenState);
  SpecialPowers.spawn(browser, [fullScreenState], async state => {
    // Wait for document focus before requesting full-screen
    await ContentTaskUtils.waitForCondition(
      () => content.browsingContext.isActive && content.document.hasFocus(),
      "Waiting for document focus"
    );
    if (state) {
      content.document.body.requestFullscreen();
    } else {
      content.document.exitFullscreen();
    }
  });
  return fullScreenChange;
}

async function testExpectFullScreenExit(browser, leaveFS, action) {
  let fsPromise = waitForFullScreenState(browser, false);
  if (leaveFS) {
    if (action) {
      await action();
    }
    await fsPromise;
    ok(true, "Should leave full-screen");
  } else {
    if (action) {
      await action();
    }
    let result = await Promise.race([
      fsPromise,
      new Promise(resolve => {
        SimpleTest.requestFlakyTimeout("Wait for failure condition");
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(() => resolve(true), 2500);
      }),
    ]);
    ok(result, "Should not leave full-screen");
  }
}

function jsWindowFocus(browser, iframeId) {
  return ContentTask.spawn(browser, { iframeId }, async args => {
    let destWin = content;
    if (args.iframeId) {
      let iframe = content.document.getElementById(args.iframeId);
      if (!iframe) {
        throw new Error("iframe not set");
      }
      destWin = iframe.contentWindow;
    }
    await content.wrappedJSObject.sendMessage(destWin, "focus");
  });
}

function jsElementFocus(browser, iframeId) {
  return ContentTask.spawn(browser, { iframeId }, async args => {
    let destWin = content;
    if (args.iframeId) {
      let iframe = content.document.getElementById(args.iframeId);
      if (!iframe) {
        throw new Error("iframe not set");
      }
      destWin = iframe.contentWindow;
    }
    await content.wrappedJSObject.sendMessage(destWin, "elementfocus");
  });
}

async function jsWindowOpen(browser, isPopup, iframeId) {
  //let windowOpened = BrowserTestUtils.waitForNewWindow();
  let windowOpened = isPopup
    ? BrowserTestUtils.waitForNewWindow()
    : BrowserTestUtils.waitForNewTab(gBrowser, null, true);
  ContentTask.spawn(browser, { isPopup, iframeId }, async args => {
    let destWin = content;
    if (args.iframeId) {
      // Create a cross origin iframe
      destWin = (
        await content.wrappedJSObject.createIframe(args.iframeId, true)
      ).contentWindow;
    }
    // Send message to either the iframe or the current page to open a popup
    await content.wrappedJSObject.sendMessage(
      destWin,
      args.isPopup ? "openpopup" : "open"
    );
  });
  return windowOpened;
}

function waitForFocus(...args) {
  return new Promise(resolve => SimpleTest.waitForFocus(resolve, ...args));
}

function waitForBrowserWindowActive(win) {
  return new Promise(resolve => {
    if (Services.focus.activeWindow == win) {
      resolve();
    } else {
      win.addEventListener(
        "activate",
        () => {
          resolve();
        },
        { once: true }
      );
    }
  });
}
