/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { Sqlite } = ChromeUtils.import("resource://gre/modules/Sqlite.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);

XPCOMUtils.defineLazyGetter(this, "TRACK_DB_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "protections.sqlite");
});

ChromeUtils.defineModuleGetter(
  this,
  "ContentBlockingAllowList",
  "resource://gre/modules/ContentBlockingAllowList.jsm"
);

var { UrlClassifierTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlClassifierTestUtils.jsm"
);
var protectionsPopup = document.getElementById("protections-popup");
var protectionsPopupMainView = document.getElementById(
  "protections-popup-mainView"
);
var protectionsPopupHeader = document.getElementById(
  "protections-popup-mainView-panel-header"
);

async function openProtectionsPanel(toast) {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    protectionsPopup,
    "popupshown"
  );
  let shieldIconContainer = document.getElementById(
    "tracking-protection-icon-container"
  );

  // Move out than move over the shield icon to trigger the hover event in
  // order to fetch tracker count.
  EventUtils.synthesizeMouseAtCenter(gURLBar.textbox, {
    type: "mousemove",
  });
  EventUtils.synthesizeMouseAtCenter(shieldIconContainer, {
    type: "mousemove",
  });

  if (!toast) {
    EventUtils.synthesizeMouseAtCenter(shieldIconContainer, {});
  } else {
    gProtectionsHandler.showProtectionsPopup({ toast });
  }

  await popupShownPromise;
}

async function openProtectionsPanelWithKeyNav() {
  let popupShownPromise = BrowserTestUtils.waitForEvent(
    protectionsPopup,
    "popupshown"
  );

  gURLBar.focus();

  // This will trigger the focus event for the shield icon for pre-fetching
  // the tracker count.
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  EventUtils.synthesizeKey("KEY_Enter", {});

  await popupShownPromise;
}

async function closeProtectionsPanel() {
  let popuphiddenPromise = BrowserTestUtils.waitForEvent(
    protectionsPopup,
    "popuphidden"
  );

  PanelMultiView.hidePopup(protectionsPopup);
  await popuphiddenPromise;
}

function checkClickTelemetry(objectName, value, source = "protectionspopup") {
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS
  ).parent;
  let buttonEvents = events.filter(
    e =>
      e[1] == `security.ui.${source}` &&
      e[2] == "click" &&
      e[3] == objectName &&
      e[4] === value
  );
  is(buttonEvents.length, 1, `recorded ${objectName} telemetry event`);
}

async function addTrackerDataIntoDB(count) {
  const insertSQL =
    "INSERT INTO events (type, count, timestamp)" +
    "VALUES (:type, :count, date(:timestamp));";

  let db = await Sqlite.openConnection({ path: TRACK_DB_PATH });
  let date = new Date().toISOString();

  await db.execute(insertSQL, {
    type: TrackingDBService.TRACKERS_ID,
    count,
    timestamp: date,
  });

  await db.close();
}

async function waitForAboutProtectionsTab() {
  let tab = await BrowserTestUtils.waitForNewTab(gBrowser, "about:protections");

  // When the graph is built it means the messaging has finished,
  // we can close the tab.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(() => {
      let bars = content.document.querySelectorAll(".graph-bar");
      return bars.length;
    }, "The graph has been built");
  });

  return tab;
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url) {
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);
  }

  return loaded;
}

function openIdentityPopup() {
  let mainView = document.getElementById("identity-popup-mainView");
  let viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  gIdentityHandler._identityBox.click();
  return viewShown;
}

function waitForSecurityChange(numChanges = 1, win = null) {
  if (!win) {
    win = window;
  }
  return new Promise(resolve => {
    let n = 0;
    let listener = {
      onSecurityChange() {
        n = n + 1;
        info("Received onSecurityChange event " + n + " of " + numChanges);
        if (n >= numChanges) {
          win.gBrowser.removeProgressListener(listener);
          resolve(n);
        }
      },
    };
    win.gBrowser.addProgressListener(listener);
  });
}

function waitForContentBlockingEvent(numChanges = 1, win = null) {
  if (!win) {
    win = window;
  }
  return new Promise(resolve => {
    let n = 0;
    let listener = {
      onContentBlockingEvent(webProgress, request, event) {
        n = n + 1;
        info(
          `Received onContentBlockingEvent event: ${event} (${n} of ${numChanges})`
        );
        if (n >= numChanges) {
          win.gBrowser.removeProgressListener(listener);
          resolve(n);
        }
      },
    };
    win.gBrowser.addProgressListener(listener);
  });
}
