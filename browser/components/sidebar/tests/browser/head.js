/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const imageBuffer = imageBufferFromDataURI(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGBgAAAABQABh6FO1AAAAABJRU5ErkJggg=="
);

function imageBufferFromDataURI(encodedImageData) {
  const decodedImageData = atob(encodedImageData);
  return Uint8Array.from(decodedImageData, byte => byte.charCodeAt(0)).buffer;
}

/* global browser */
const extData = {
  manifest: {
    sidebar_action: {
      default_icon: {
        16: "icon.png",
        32: "icon@2x.png",
      },
      default_panel: "default.html",
      default_title: "Default Title",
    },
  },
  useAddonManager: "temporary",

  files: {
    "default.html": `
          <!DOCTYPE html>
          <html>
          <head><meta charset="utf-8"/>
          <script src="sidebar.js"></script>
          </head>
          <body>
          A Test Sidebar
          </body></html>
        `,
    "sidebar.js": function () {
      window.onload = () => {
        browser.test.sendMessage("sidebar");
      };
    },
    "1.html": `
          <!DOCTYPE html>
          <html>
          <head><meta charset="utf-8"/></head>
          <body>
          A Test Sidebar
          </body></html>
        `,
    "icon.png": imageBuffer,
    "icon@2x.png": imageBuffer,
    "updated-icon.png": imageBuffer,
  },

  background() {
    browser.test.onMessage.addListener(async ({ msg, data }) => {
      switch (msg) {
        case "set-icon":
          await browser.sidebarAction.setIcon({ path: data });
          break;
        case "set-panel":
          await browser.sidebarAction.setPanel({ panel: data });
          break;
        case "set-title":
          await browser.sidebarAction.setTitle({ title: data });
          break;
      }
      browser.test.sendMessage("done");
    });
  },
};

function waitForBrowserWindowActive(win) {
  // eslint-disable-next-line consistent-return
  return new Promise(resolve => {
    if (Services.focus.activeWindow == win) {
      resolve();
    } else {
      return BrowserTestUtils.waitForEvent(win, "activate");
    }
  });
}

function openAndWaitForContextMenu(popup, button, onShown, onHidden) {
  return new Promise(resolve => {
    function onPopupShown() {
      info("onPopupShown");
      popup.removeEventListener("popupshown", onPopupShown);

      onShown && onShown();

      // Use setTimeout() to get out of the popupshown event.
      popup.addEventListener("popuphidden", onPopupHidden);
      setTimeout(() => popup.hidePopup(), 0);
    }
    function onPopupHidden() {
      info("onPopupHidden");
      popup.removeEventListener("popuphidden", onPopupHidden);

      onHidden && onHidden();

      resolve(popup);
    }

    popup.addEventListener("popupshown", onPopupShown);

    info("wait for the context menu to open");

    button.scrollIntoView();
    const eventDetails = { type: "contextmenu", button: 2 };
    EventUtils.synthesizeMouse(
      button,
      5,
      2,
      eventDetails,
      // eslint-disable-next-line mozilla/use-ownerGlobal
      button.ownerDocument.defaultView
    );
  });
}

function isActiveElement(el) {
  return el.getRootNode().activeElement == el;
}
