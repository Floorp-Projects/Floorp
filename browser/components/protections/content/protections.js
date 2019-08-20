/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

import LockwiseCard from "./lockwise-card.js";
import MonitorCard from "./monitor-card.js";

// We need to send the close telemetry before unload while we still have a connection to RPM.
window.addEventListener("beforeunload", () => {
  document.sendTelemetryEvent("close", "protection_report");
});

document.addEventListener("DOMContentLoaded", e => {
  let todayInMs = Date.now();
  let weekAgoInMs = todayInMs - 7 * 24 * 60 * 60 * 1000;
  RPMSendAsyncMessage("FetchContentBlockingEvents", {
    from: weekAgoInMs,
    to: todayInMs,
  });
  let dataTypes = [
    "cryptominer",
    "fingerprinter",
    "tracker",
    "cookie",
    "social",
  ];

  let protectionDetails = document.getElementById("protection-details");
  let protectionDetailsEvtHandler = evt => {
    if (evt.keyCode == evt.DOM_VK_RETURN || evt.type == "click") {
      RPMSendAsyncMessage("OpenContentBlockingPreferences");
    }
  };
  protectionDetails.addEventListener("click", protectionDetailsEvtHandler);
  protectionDetails.addEventListener("keypress", protectionDetailsEvtHandler);

  let cbCategory = RPMGetStringPref("browser.contentblocking.category");
  if (cbCategory == "custom") {
    protectionDetails.setAttribute(
      "data-l10n-id",
      "protection-header-details-custom"
    );
  } else if (cbCategory == "strict") {
    protectionDetails.setAttribute(
      "data-l10n-id",
      "protection-header-details-strict"
    );
  } else {
    protectionDetails.setAttribute(
      "data-l10n-id",
      "protection-header-details-standard"
    );
  }

  let legend = document.getElementById("legend");
  legend.style.gridTemplateAreas =
    "'social cookie tracker fingerprinter cryptominer'";

  document.sendTelemetryEvent = (action, object, value = "") => {
    // eslint-disable-next-line no-undef
    RPMRecordTelemetryEvent("security.ui.protections", action, object, value, {
      category: cbCategory,
    });
  };

  // Send telemetry on arriving and closing this page
  document.sendTelemetryEvent("show", "protection_report");

  let createGraph = data => {
    // All of our dates are recorded as 00:00 GMT, add 12 hours to the timestamp
    // to ensure we display the correct date no matter the user's location.
    let hoursInMS12 = 12 * 60 * 60 * 1000;
    let dateInMS = data.earliestDate
      ? new Date(data.earliestDate).getTime() + hoursInMS12
      : Date.now();

    let summary = document.getElementById("graph-total-summary");
    summary.setAttribute(
      "data-l10n-args",
      JSON.stringify({ count: data.sumEvents, earliestDate: dateInMS })
    );
    summary.setAttribute("data-l10n-id", "graph-total-summary");

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
    // The graph is already a role "table" from the HTML file.
    let graph = document.getElementById("graph");
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
        bar.appendChild(count);
        ariaOwnsString = count.id;
        currentColumnCount += 1;
        let barHeight = (content.total / largest) * 100;
        weekCount += content.total;
        bar.style.height = `${barHeight}%`;
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
            div.setAttribute(
              "data-l10n-args",
              JSON.stringify({ count: content[type], percentage: dataHeight })
            );
            div.setAttribute("data-l10n-id", `bar-tooltip-${type}`);
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
      let weekSummary = document.getElementById("graph-week-summary");
      weekSummary.setAttribute(
        "data-l10n-args",
        JSON.stringify({ count: weekCount })
      );
      weekSummary.setAttribute("data-l10n-id", "graph-week-summary");

      let label = document.createElement("span");
      label.className = "column-label";
      // While the graphs fill up from the right, the days fill up from the left, so match the IDs
      label.id = "day" + (6 - i);
      label.setAttribute("role", "rowheader");
      if (i == 6) {
        label.setAttribute("data-l10n-id", "graph-today");
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
      document.querySelector(`label[data-type=${type}]`).textContent =
        weekTypeCounts[type];
      const learnMoreLink = document.getElementById(`${type}-link`);
      learnMoreLink.href = RPMGetFormatURLPref(
        `browser.contentblocking.report.${type}.url`
      );
      learnMoreLink.addEventListener("click", () => {
        document.sendTelemetryEvent("click", "trackers_about_link", type);
      });
    }

    // Hide the trackers tab if the user is in standard and
    // has no recorded trackers blocked.
    if (weekTypeCounts.tracker == 0 && cbCategory == "standard") {
      legend.style.gridTemplateAreas = legend.style.gridTemplateAreas.replace(
        "tracker",
        ""
      );
      let radio = document.getElementById("tab-tracker");
      radio.setAttribute("disabled", true);
      document.querySelector("#tab-tracker ~ label").style.display = "none";
    }
    let socialEnabled = RPMGetBoolPref(
      "privacy.socialtracking.block_cookies.enabled",
      false
    );

    if (weekTypeCounts.social == 0 && !socialEnabled) {
      legend.style.gridTemplateAreas = legend.style.gridTemplateAreas.replace(
        "social",
        ""
      );
      let radio = document.getElementById("tab-social");
      radio.setAttribute("disabled", true);
      document.querySelector("#tab-social ~ label").style.display = "none";
    }

    let firstRadio = document.querySelector("input:not(:disabled)");
    firstRadio.checked = true;
    document.body.setAttribute("focuseddatatype", firstRadio.dataset.type);

    addListeners();
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

  RPMAddMessageListener("SendContentBlockingRecords", message => {
    createGraph(message.data);
  });

  let lockwiseEnabled = RPMGetBoolPref(
    "browser.contentblocking.report.lockwise.enabled",
    true
  );
  if (lockwiseEnabled) {
    const lockwiseUI = document.querySelector(".lockwise-card");
    lockwiseUI.classList.remove("hidden");
    lockwiseUI.classList.add("loading");

    const lockwiseCard = new LockwiseCard(document);
    lockwiseCard.init();
  }

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

  // Dispatch messages to retrieve data for the Lockwise & Monitor
  // cards.
  RPMSendAsyncMessage("FetchUserLoginsData");
});
