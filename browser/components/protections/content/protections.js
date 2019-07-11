/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

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
  let weekdays = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

  let protectionDetails = document.getElementById("protection-details");
  protectionDetails.addEventListener("click", () => {
    RPMSendAsyncMessage("OpenContentBlockingPreferences");
  });

  let createGraph = data => {
    // Set a default top size for the height of the graph bars so that small
    // numbers don't fill the whole graph.
    let largest = 100;
    if (largest < data.largest) {
      largest = data.largest;
    }

    let graph = document.getElementById("graph");
    for (let i = weekdays.length - 1; i >= 0; i--) {
      // Start 7 days ago and count down to today.
      let date = new Date();
      date.setDate(date.getDate() - i);
      let dateString = date.toISOString().split("T")[0];
      let bar = document.createElement("div");
      bar.className = "graph-bar";
      if (data[dateString]) {
        let content = data[dateString];
        let barHeight = (content.total / largest) * 100;
        bar.style.height = `${barHeight}%`;
        for (let type of dataTypes) {
          if (content[type]) {
            let dataHeight = (content[type] / content.total) * 100;
            let div = document.createElement("div");
            div.className = `${type}-bar`;
            div.setAttribute("data-type", type);
            div.style.height = `${dataHeight}%`;
            bar.appendChild(div);
          }
        }
      } else {
        // There were no content blocking events on this day.
        bar.style.height = `0`;
      }
      graph.appendChild(bar);

      let label = document.createElement("span");
      label.className = "column-label";
      if (i == 6) {
        label.textContent = "Today";
      } else {
        label.textContent = weekdays[(i + 1 + new Date().getDay()) % 7];
      }
      graph.prepend(label);
    }

    addListeners();
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
      if (ev.originalTarget.dataset.type) {
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

  RPMAddMessageListener("SendContentBlockingRecords", message => {
    createGraph(message.data);
  });
});
