/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/remote-page */

import LockwiseCard from "./lockwise-card.mjs";
import MonitorCard from "./monitor-card.mjs";
import ProxyCard from "./proxy-card.mjs";
import VPNCard from "./vpn-card.mjs";

let cbCategory = RPMGetStringPref("browser.contentblocking.category");
document.sendTelemetryEvent = (action, object, value = "") => {
  // eslint-disable-next-line no-undef
  RPMRecordTelemetryEvent("security.ui.protections", action, object, value, {
    category: cbCategory,
  });
};

let { protocol, pathname, searchParams } = new URL(document.location);

let searchParamsChanged = false;
if (searchParams.has("entrypoint")) {
  RPMSendAsyncMessage("RecordEntryPoint", {
    entrypoint: searchParams.get("entrypoint"),
  });
  // Remove this parameter from the URL (after recording above) to make it
  // cleaner for bookmarking and switch-to-tab and so that bookmarked values
  // don't skew telemetry.
  searchParams.delete("entrypoint");
  searchParamsChanged = true;
}

document.addEventListener("DOMContentLoaded", e => {
  if (searchParamsChanged) {
    let newURL = protocol + pathname;
    let params = searchParams.toString();
    if (params) {
      newURL += "?" + params;
    }
    window.location.replace(newURL);
    return;
  }

  RPMSendQuery("FetchEntryPoint", {}).then(entrypoint => {
    // Send telemetry on arriving on this page
    document.sendTelemetryEvent("show", "protection_report", entrypoint);
  });

  // We need to send the close telemetry before unload while we still have a connection to RPM.
  window.addEventListener("beforeunload", () => {
    document.sendTelemetryEvent("close", "protection_report");
  });

  let todayInMs = Date.now();
  let weekAgoInMs = todayInMs - 6 * 24 * 60 * 60 * 1000;

  let dataTypes = [
    "cryptominer",
    "fingerprinter",
    "tracker",
    "cookie",
    "social",
  ];

  let manageProtectionsLink = document.getElementById("protection-settings");
  let manageProtections = document.getElementById("manage-protections");
  let protectionSettingsEvtHandler = evt => {
    if (evt.keyCode == evt.DOM_VK_RETURN || evt.type == "click") {
      RPMSendAsyncMessage("OpenContentBlockingPreferences");
      if (evt.target.id == "protection-settings") {
        document.sendTelemetryEvent(
          "click",
          "settings_link",
          "header-settings"
        );
      } else if (evt.target.id == "manage-protections") {
        document.sendTelemetryEvent(
          "click",
          "settings_link",
          "custom-card-settings"
        );
      }
    }
  };
  manageProtectionsLink.addEventListener("click", protectionSettingsEvtHandler);
  manageProtectionsLink.addEventListener(
    "keypress",
    protectionSettingsEvtHandler
  );
  manageProtections.addEventListener("click", protectionSettingsEvtHandler);
  manageProtections.addEventListener("keypress", protectionSettingsEvtHandler);

  let legend = document.getElementById("legend");
  legend.style.gridTemplateAreas =
    "'social cookie tracker fingerprinter cryptominer'";

  let createGraph = data => {
    let graph = document.getElementById("graph");
    let summary = document.getElementById("graph-total-summary");
    let weekSummary = document.getElementById("graph-week-summary");

    // User is in private mode, show no data on the graph
    if (data.isPrivate) {
      graph.classList.add("private-window");
    } else {
      let earliestDate = data.earliestDate || Date.now();
      document.l10n.setAttributes(summary, "graph-total-tracker-summary", {
        count: data.sumEvents,
        earliestDate,
      });
    }

    // Set a default top size for the height of the graph bars so that small
    // numbers don't fill the whole graph.
    let largest = 100;
    if (largest < data.largest) {
      largest = data.largest;
    }
    let weekCount = 0;
    let weekTypeCounts = {
      social: 0,
      cookie: 0,
      tracker: 0,
      fingerprinter: 0,
      cryptominer: 0,
    };

    // For accessibility clients, we turn the graph into a fake table with annotated text.
    // We use WAI-ARIA roles, properties, and states to mark up the table, rows and cells.
    // Each day becomes one row in the table.
    // Each row contains the day, total, and then one cell for each bar that we display.
    // At most, a row can contain seven cells.
    // But we need to caclulate the actual number of the most cells in a row to give accurate information.
    let maxColumnCount = 0;
    let date = new Date();
    for (let i = 0; i <= 6; i++) {
      let dateString = date.toISOString().split("T")[0];
      let ariaOwnsString = ""; // Get the row's colummns in order
      let currentColumnCount = 0;
      let bar = document.createElement("div");
      bar.className = "graph-bar";
      bar.setAttribute("role", "row");
      let innerBar = document.createElement("div");
      innerBar.className = "graph-wrapper-bar";
      if (data[dateString]) {
        let content = data[dateString];
        let count = document.createElement("div");
        count.className = "bar-count";
        count.id = "count" + i;
        count.setAttribute("role", "cell");
        count.textContent = content.total;
        setTimeout(() => {
          count.classList.add("animate");
        }, 400);
        bar.appendChild(count);
        ariaOwnsString = count.id;
        currentColumnCount += 1;
        let barHeight = (content.total / largest) * 100;
        weekCount += content.total;
        // Add a short timeout to allow the elements to be added to the dom before triggering an animation.
        setTimeout(() => {
          bar.style.height = `${barHeight}%`;
        }, 20);
        for (let type of dataTypes) {
          if (content[type]) {
            let dataHeight = (content[type] / content.total) * 100;
            // Since we are dealing with non-visual content, screen readers need a parent container to get the text
            let cellSpan = document.createElement("span");
            cellSpan.id = type + i;
            cellSpan.setAttribute("role", "cell");
            let div = document.createElement("div");
            div.className = `${type}-bar inner-bar`;
            div.setAttribute("role", "img");
            div.setAttribute("data-type", type);
            div.style.height = `${dataHeight}%`;
            document.l10n.setAttributes(div, `bar-tooltip-${type}`, {
              count: content[type],
              percentage: dataHeight,
            });
            weekTypeCounts[type] += content[type];
            cellSpan.appendChild(div);
            innerBar.appendChild(cellSpan);
            ariaOwnsString = ariaOwnsString + " " + cellSpan.id;
            currentColumnCount += 1;
          }
        }
        if (currentColumnCount > maxColumnCount) {
          // The current row has more than any previous rows
          maxColumnCount = currentColumnCount;
        }
      } else {
        // There were no content blocking events on this day.
        bar.classList.add("empty");
      }
      bar.appendChild(innerBar);
      graph.prepend(bar);

      if (data.isPrivate) {
        document.l10n.setAttributes(
          weekSummary,
          "graph-week-summary-private-window"
        );
      } else {
        document.l10n.setAttributes(weekSummary, "graph-week-summary", {
          count: weekCount,
        });
      }

      let label = document.createElement("span");
      label.className = "column-label";
      // While the graphs fill up from the right, the days fill up from the left, so match the IDs
      label.id = "day" + (6 - i);
      label.setAttribute("role", "rowheader");
      if (i == 6) {
        document.l10n.setAttributes(label, "graph-today");
      } else {
        label.textContent = data.weekdays[(i + 1 + new Date().getDay()) % 7];
      }
      graph.append(label);
      // Make the day the first column in a row, making it the row header.
      bar.setAttribute("aria-owns", "day" + i + " " + ariaOwnsString);
      date.setDate(date.getDate() - 1);
    }
    maxColumnCount += 1; // Add the day column in the fake table
    graph.setAttribute("aria-colCount", maxColumnCount);
    // Set the total number of each type of tracker on the tabs as well as their
    // "Learn More" links
    for (let type of dataTypes) {
      document.querySelector(`label[data-type=${type}] span`).textContent =
        weekTypeCounts[type];
      const learnMoreLink = document.getElementById(`${type}-link`);
      learnMoreLink.href = RPMGetFormatURLPref(
        `browser.contentblocking.report.${type}.url`
      );
      learnMoreLink.addEventListener("click", () => {
        document.sendTelemetryEvent("click", "trackers_about_link", type);
      });
    }

    let blockingCookies =
      RPMGetIntPref("network.cookie.cookieBehavior", 0) != 0;
    let cryptominingEnabled = RPMGetBoolPref(
      "privacy.trackingprotection.cryptomining.enabled",
      false
    );
    let fingerprintingEnabled = RPMGetBoolPref(
      "privacy.trackingprotection.fingerprinting.enabled",
      false
    );
    let tpEnabled = RPMGetBoolPref("privacy.trackingprotection.enabled", false);
    let socialTracking = RPMGetBoolPref(
      "privacy.trackingprotection.socialtracking.enabled",
      false
    );
    let socialCookies = RPMGetBoolPref(
      "privacy.socialtracking.block_cookies.enabled",
      false
    );
    let socialEnabled =
      socialCookies && (blockingCookies || (tpEnabled && socialTracking));
    let notBlocking =
      !blockingCookies &&
      !cryptominingEnabled &&
      !fingerprintingEnabled &&
      !tpEnabled &&
      !socialEnabled;

    // User has turned off all blocking, show a different card.
    if (notBlocking) {
      document.l10n.setAttributes(
        document.getElementById("etp-card-content"),
        "protection-report-etp-card-content-custom-not-blocking"
      );
      document.l10n.setAttributes(
        document.querySelector(".etp-card .card-title"),
        "etp-card-title-custom-not-blocking"
      );
      document.l10n.setAttributes(
        document.getElementById("report-summary"),
        "protection-report-page-summary"
      );
      document.querySelector(".etp-card").classList.add("custom-not-blocking");

      // Hide the link to settings from the header, so we are not showing two links.
      manageProtectionsLink.style.display = "none";
    } else {
      // Hide each type of tab if blocking of that type is off.
      if (!tpEnabled) {
        legend.style.gridTemplateAreas = legend.style.gridTemplateAreas.replace(
          "tracker",
          ""
        );
        let radio = document.getElementById("tab-tracker");
        radio.setAttribute("disabled", true);
        document.querySelector("#tab-tracker ~ label").style.display = "none";
      }
      if (!socialEnabled) {
        legend.style.gridTemplateAreas = legend.style.gridTemplateAreas.replace(
          "social",
          ""
        );
        let radio = document.getElementById("tab-social");
        radio.setAttribute("disabled", true);
        document.querySelector("#tab-social ~ label").style.display = "none";
      }
      if (!blockingCookies) {
        legend.style.gridTemplateAreas = legend.style.gridTemplateAreas.replace(
          "cookie",
          ""
        );
        let radio = document.getElementById("tab-cookie");
        radio.setAttribute("disabled", true);
        document.querySelector("#tab-cookie ~ label").style.display = "none";
      }
      if (!cryptominingEnabled) {
        legend.style.gridTemplateAreas = legend.style.gridTemplateAreas.replace(
          "cryptominer",
          ""
        );
        let radio = document.getElementById("tab-cryptominer");
        radio.setAttribute("disabled", true);
        document.querySelector("#tab-cryptominer ~ label").style.display =
          "none";
      }
      if (!fingerprintingEnabled) {
        legend.style.gridTemplateAreas = legend.style.gridTemplateAreas.replace(
          "fingerprinter",
          ""
        );
        let radio = document.getElementById("tab-fingerprinter");
        radio.setAttribute("disabled", true);
        document.querySelector("#tab-fingerprinter ~ label").style.display =
          "none";
      }

      let firstRadio = document.querySelector("input:enabled");
      // There will be no radio options if we are showing the
      firstRadio.checked = true;
      document.body.setAttribute("focuseddatatype", firstRadio.dataset.type);

      addListeners();
    }
  };

  let addListeners = () => {
    let wrapper = document.querySelector(".body-wrapper");
    let triggerTabClick = ev => {
      if (ev.originalTarget.dataset.type) {
        document.getElementById(`tab-${ev.target.dataset.type}`).click();
      }
    };

    let triggerTabFocus = ev => {
      if (ev.originalTarget.dataset) {
        wrapper.classList.add("hover-" + ev.originalTarget.dataset.type);
      }
    };

    let triggerTabBlur = ev => {
      if (ev.originalTarget.dataset) {
        wrapper.classList.remove("hover-" + ev.originalTarget.dataset.type);
      }
    };
    wrapper.addEventListener("mouseout", triggerTabBlur);
    wrapper.addEventListener("mouseover", triggerTabFocus);
    wrapper.addEventListener("click", triggerTabClick);

    // Change the class on the body to change the color variable.
    let radios = document.querySelectorAll("#legend input");
    for (let radio of radios) {
      radio.addEventListener("change", ev => {
        document.body.setAttribute("focuseddatatype", ev.target.dataset.type);
      });
      radio.addEventListener("focus", ev => {
        wrapper.classList.add("hover-" + ev.originalTarget.dataset.type);
        document.body.setAttribute("focuseddatatype", ev.target.dataset.type);
      });
      radio.addEventListener("blur", ev => {
        wrapper.classList.remove("hover-" + ev.originalTarget.dataset.type);
      });
    }
  };

  RPMSendQuery("FetchContentBlockingEvents", {
    from: weekAgoInMs,
    to: todayInMs,
  }).then(createGraph);

  let exitIcon = document.querySelector("#mobile-hanger .exit-icon");
  // hide the mobile promotion and keep hidden with a pref.
  exitIcon.addEventListener("click", () => {
    RPMSetPref("browser.contentblocking.report.show_mobile_app", false);
    document.getElementById("mobile-hanger").classList.add("hidden");
  });

  let androidMobileAppLink = document.getElementById(
    "android-mobile-inline-link"
  );
  androidMobileAppLink.href = RPMGetStringPref(
    "browser.contentblocking.report.mobile-android.url"
  );
  androidMobileAppLink.addEventListener("click", () => {
    document.sendTelemetryEvent("click", "mobile_app_link", "android");
  });
  let iosMobileAppLink = document.getElementById("ios-mobile-inline-link");
  iosMobileAppLink.href = RPMGetStringPref(
    "browser.contentblocking.report.mobile-ios.url"
  );
  iosMobileAppLink.addEventListener("click", () => {
    document.sendTelemetryEvent("click", "mobile_app_link", "ios");
  });

  let lockwiseEnabled = RPMGetBoolPref(
    "browser.contentblocking.report.lockwise.enabled",
    true
  );

  let lockwiseCard;
  if (lockwiseEnabled) {
    const lockwiseUI = document.querySelector(".lockwise-card");
    lockwiseUI.classList.remove("hidden");
    lockwiseUI.classList.add("loading");

    lockwiseCard = new LockwiseCard(document);
    lockwiseCard.init();
  }

  RPMSendQuery("FetchUserLoginsData", {}).then(data => {
    if (lockwiseCard) {
      // Once data for the user is retrieved, display the lockwise card.
      lockwiseCard.buildContent(data);
    }

    if (
      RPMGetBoolPref("browser.contentblocking.report.show_mobile_app") &&
      !data.mobileDeviceConnected
    ) {
      document
        .getElementById("mobile-hanger")
        .classList.toggle("hidden", false);
    }
  });

  // For tests
  const lockwiseUI = document.querySelector(".lockwise-card");
  lockwiseUI.dataset.enabled = lockwiseEnabled;

  let monitorEnabled = RPMGetBoolPref(
    "browser.contentblocking.report.monitor.enabled",
    true
  );
  if (monitorEnabled) {
    // Show the Monitor card.
    const monitorUI = document.querySelector(".card.monitor-card.hidden");
    monitorUI.classList.remove("hidden");
    monitorUI.classList.add("loading");

    const monitorCard = new MonitorCard(document);
    monitorCard.init();
  }

  // For tests
  const monitorUI = document.querySelector(".monitor-card");
  monitorUI.dataset.enabled = monitorEnabled;

  const proxyEnabled = RPMGetBoolPref(
    "browser.contentblocking.report.proxy.enabled",
    true
  );

  if (proxyEnabled) {
    const proxyCard = new ProxyCard(document);
    proxyCard.init();
  }

  // For tests
  const proxyUI = document.querySelector(".proxy-card");
  proxyUI.dataset.enabled = proxyEnabled;

  const VPNEnabled = RPMGetBoolPref("browser.vpn_promo.enabled", true);
  if (VPNEnabled) {
    const vpnCard = new VPNCard(document);
    vpnCard.init();
  }
  // For tests
  const vpnUI = document.querySelector(".vpn-card");
  vpnUI.dataset.enabled = VPNEnabled;
});
