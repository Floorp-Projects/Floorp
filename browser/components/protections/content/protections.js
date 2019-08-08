/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

import LockwiseCard from "./lockwise-card.js";
import MonitorCard from "./monitor-card.js";

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

    let date = new Date();
    let graph = document.getElementById("graph");
    for (let i = 0; i <= 6; i++) {
      let dateString = date.toISOString().split("T")[0];

      let bar = document.createElement("div");
      bar.className = "graph-bar";
      let innerBar = document.createElement("div");
      innerBar.className = "graph-wrapper-bar";
      if (data[dateString]) {
        let content = data[dateString];
        let count = document.createElement("div");
        count.className = "bar-count";
        count.textContent = content.total;
        bar.appendChild(count);
        let barHeight = (content.total / largest) * 100;
        weekCount += content.total;
        bar.style.height = `${barHeight}%`;
        for (let type of dataTypes) {
          if (content[type]) {
            let dataHeight = (content[type] / content.total) * 100;
            let div = document.createElement("div");
            div.className = `${type}-bar inner-bar`;
            div.setAttribute("data-type", type);
            div.style.height = `${dataHeight}%`;
            weekTypeCounts[type] += content[type];
            innerBar.appendChild(div);
          }
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

      // Set the total number of each type of tracker on the tabs
      for (let type of dataTypes) {
        document.querySelector(`label[data-type=${type}]`).textContent =
          weekTypeCounts[type];
      }

      let label = document.createElement("span");
      label.className = "column-label";
      if (i == 6) {
        label.setAttribute("data-l10n-id", "graph-today");
      } else {
        label.textContent = data.weekdays[(i + 1 + new Date().getDay()) % 7];
      }
      graph.append(label);
      date.setDate(date.getDate() - 1);
    }

    let legend = document.getElementById("legend");
    // This will change when we remove trackers from standard.
    legend.style.gridTemplateAreas =
      "'social cookie tracker fingerprinter cryptominer'";

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
