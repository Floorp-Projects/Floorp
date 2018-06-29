/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

// Clear the network cache between every test to make sure we get a stable state
Services.cache2.clear();

function waitForFaviconMessage(isTabIcon = undefined, expectedURL = undefined) {
  return new Promise((resolve, reject) => {
    let listener = msg => {
      // If requested filter out loads of the wrong kind of icon.
      if (isTabIcon != undefined && isTabIcon != msg.data.canUseForTab) {
        return;
      }

      if (expectedURL && msg.data.originalURL != expectedURL) {
        return;
      }

      window.messageManager.removeMessageListener("Link:SetIcon", listener);
      window.messageManager.removeMessageListener("Link:SetFailedIcon", listener);

      if (msg.name == "Link:SetIcon") {
        resolve({
          iconURL: msg.data.originalURL,
          dataURL: msg.data.iconURL,
          canUseForTab: msg.data.canUseForTab,
        });
      } else {
        reject({
          iconURL: msg.data.originalURL,
          canUseForTab: msg.data.canUseForTab,
        });
      }
    };

    window.messageManager.addMessageListener("Link:SetIcon", listener);
    window.messageManager.addMessageListener("Link:SetFailedIcon", listener);
  });
}

function waitForFavicon(browser, url) {
  return new Promise(resolve => {
    let listener = {
      onLinkIconAvailable(b, dataURI, iconURI) {
        if (b !== browser || iconURI != url) {
          return;
        }

        gBrowser.removeTabsProgressListener(listener);
        resolve();
      }
    };

    gBrowser.addTabsProgressListener(listener);
  });
}

function waitForLinkAvailable(browser) {
  let resolve, reject;

  let listener = {
    onLinkIconAvailable(b, dataURI, iconURI) {
      // Ignore icons for other browsers or empty icons.
      if (browser !== b || !iconURI) {
        return;
      }

      gBrowser.removeTabsProgressListener(listener);
      resolve(iconURI);
    }
  };

  let promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;

    gBrowser.addTabsProgressListener(listener);
  });

  promise.cancel = () => {
    gBrowser.removeTabsProgressListener(listener);

    reject();
  };

  return promise;
}
