/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

document.addEventListener("DOMContentLoaded", e => {
  let dataTypes = [
    "cryptominer",
    "fingerprinter",
    "tracker",
    "crossSite",
    "social",
  ];
  let weekdays = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
  let today = new Date().getDay();

  let protectionDetails = document.getElementById("protection-details");
  protectionDetails.addEventListener("click", () => {
    RPMSendAsyncMessage("openContentBlockingPreferences");
  });

  let data = [
    {
      total: 41,
      cryptominer: 1,
      fingerprinter: 10,
      tracker: 15,
      crossSite: 12,
      social: 3,
    },
    {
      total: 246,
      cryptominer: 5,
      fingerprinter: 8,
      tracker: 110,
      crossSite: 103,
      social: 20,
    },
    {
      total: 59,
      cryptominer: 0,
      fingerprinter: 1,
      tracker: 25,
      crossSite: 25,
      social: 8,
    },
    {
      total: 177,
      cryptominer: 0,
      fingerprinter: 4,
      tracker: 24,
      crossSite: 136,
      social: 13,
    },
    {
      total: 16,
      cryptominer: 1,
      fingerprinter: 3,
      tracker: 0,
      crossSite: 7,
      social: 5,
    },
    {
      total: 232,
      cryptominer: 0,
      fingerprinter: 30,
      tracker: 84,
      crossSite: 86,
      social: 32,
    },
    {
      total: 153,
      cryptominer: 0,
      fingerprinter: 10,
      tracker: 35,
      crossSite: 95,
      social: 13,
    },
  ];

  // Use this to populate the graph with real data in the future.
  let createGraph = () => {
    let largest = 10;
    for (let day of data) {
      if (largest < day.total) {
        largest = day.total;
      }
    }

    let graph = document.getElementById("graph");
    for (let i = 0; i < weekdays.length; i++) {
      let bar = document.createElement("div");
      bar.className = "graph-bar";
      let barHeight = (data[i].total / largest) * 100;
      bar.style.height = `${barHeight}%`;
      for (let type of dataTypes) {
        let dataHeight = (data[i][type] / data[i].total) * 100;
        let div = document.createElement("div");
        div.className = `${type}-bar`;
        div.setAttribute("data-type", type);
        div.style.height = `${dataHeight}%`;
        bar.appendChild(div);
      }
      graph.appendChild(bar);

      let label = document.createElement("span");
      label.className = "column-label";
      if (i == 6) {
        label.innerText = "Today";
      } else {
        label.innerText = weekdays[(i + today) % 7];
      }
      graph.appendChild(label);
    }
  };

  let addListeners = () => {
    let wrapper = document.querySelector(".body-wrapper");
    wrapper.addEventListener("mouseover", ev => {
      if (ev.originalTarget.dataset) {
        wrapper.classList.add("hover-" + ev.originalTarget.dataset.type);
      }
    });

    wrapper.addEventListener("mouseout", ev => {
      if (ev.originalTarget.dataset) {
        wrapper.classList.remove("hover-" + ev.originalTarget.dataset.type);
      }
    });

    wrapper.addEventListener("click", ev => {
      if (ev.originalTarget.dataset) {
        document.getElementById(`tab-${ev.target.dataset.type}`).click();
      }
    });

    // Change the class on the body to change the color variable.
    let radios = document.querySelectorAll("#legend input");
    for (let radio of radios) {
      radio.addEventListener("change", ev => {
        document.body.setAttribute("focuseddatatype", ev.target.dataset.type);
      });
    }
  };
  createGraph();
  addListeners();
});
