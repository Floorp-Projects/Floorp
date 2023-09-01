/* eslint-disable no-undef */

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function extractColorsFromBase64Image(base64Image) {
  if (!base64Image) {
    return Promise.reject("No image provided");
  }

  const binaryData = atob(base64Image);
  const byteArray = new Uint8Array(binaryData.length);

  for (let i = 0; i < binaryData.length; i++) {
    byteArray[i] = binaryData.charCodeAt(i);
  }

  const canvas = document.createElement("canvas");
  const ctx = canvas.getContext("2d");
  const img = new Image();

  return new Promise((resolve, reject) => {
    img.onload = () => {
      canvas.width = img.width;
      canvas.height = img.height;
      ctx.drawImage(img, 0, 0);

      const pixelData = ctx.getImageData(
        0,
        0,
        canvas.width,
        canvas.height
      ).data;
      const colors = [];
      let totalRed = 0;
      let totalGreen = 0;
      let totalBlue = 0;
      const redCounts = {};
      const greenCounts = {};
      const blueCounts = {};

      for (let i = 0; i < pixelData.length; i += 4) {
        const red = pixelData[i];
        const green = pixelData[i + 1];
        const blue = pixelData[i + 2];

        if (
          (red === 255 && green === 255 && blue === 255) ||
          (red === 0 && green === 0 && blue === 0)
        ) {
          continue;
        }

        totalRed += red;
        totalGreen += green;
        totalBlue += blue;

        const color = `rgb(${red}, ${green}, ${blue})`;
        colors.push(color);

        redCounts[red] = (redCounts[red] || 0) + 1;
        greenCounts[green] = (greenCounts[green] || 0) + 1;
        blueCounts[blue] = (blueCounts[blue] || 0) + 1;
      }

      const pixelCount = pixelData.length / 4;
      const averageRed = Math.round(totalRed / pixelCount);
      const averageGreen = Math.round(totalGreen / pixelCount);
      const averageBlue = Math.round(totalBlue / pixelCount);
      const averageColor = `rgb(${averageRed}, ${averageGreen}, ${averageBlue})`;

      const mostFrequentRed = findMostFrequentValue(redCounts);
      const mostFrequentGreen = findMostFrequentValue(greenCounts);
      const mostFrequentBlue = findMostFrequentValue(blueCounts);
      const mostFrequentColor = `rgb(${mostFrequentRed}, ${mostFrequentGreen}, ${mostFrequentBlue})`;

      resolve({
        colors,
        averageColor,
        mostFrequentColor,
      });
    };

    img.onerror = reject;

    img.src = URL.createObjectURL(new Blob([byteArray]));
  });
}

function findMostFrequentValue(counts) {
  let mostFrequentValue;
  let maxCount = 0;

  for (const value in counts) {
    if (counts[value] > maxCount) {
      maxCount = counts[value];
      mostFrequentValue = value;
    }
  }

  return parseInt(mostFrequentValue);
}

function setFaviconColorToTitlebar() {
  const base64Image = document.querySelector(
    '.tab-icon-image[selected="true"]'
  )?.src;
  const base64ImageWithoutHeader = base64Image?.split(",")[1];

  extractColorsFromBase64Image(base64ImageWithoutHeader)
    .then(result => {
      let elems = document.querySelectorAll(".floorp-toolbar-bgcolor");
      for (let i = 0; i < elems.length; i++) {
        elems[i].remove();
      }

      let elem = document.createElement("style");
      let textColor =
        result.averageColor - 0x222222 > 0xffffff / 2 ? "#000000" : "#ffffff";
      let CSS = `
        #navigator-toolbox:not(:-moz-lwtheme), #navigator-toolbox:-moz-lwtheme {
          background-color: ${result.averageColor} !important;
        }

        .tab-label:not([selected="true"]) {
          color: ${textColor} !important;
        }

        .tab-icon-stack > * , #TabsToolbar-customization-target > *, #tabs-newtab-button, .titlebar-color > * {
          color: ${textColor} !important;
          fill: ${textColor} !important;
        }
      `;
      elem.textContent = CSS;
      elem.className = "floorp-toolbar-bgcolor";
      document.head.appendChild(elem);
    })
    .catch(error => {
      let elems = document.querySelectorAll(".floorp-toolbar-bgcolor");
      for (let i = 0; i < elems.length; i++) {
        elems[i].remove();
      }
    });
}

function enableFaviconColorToTitlebar() {
  setFaviconColorToTitlebar();
  gBrowser.tabContainer.addEventListener(
    "TabSelect",
    setFaviconColorToTitlebar
  );
  gBrowser.tabContainer.addEventListener("TabMove", setFaviconColorToTitlebar);
  gBrowser.tabContainer.addEventListener(
    "TabAttrModified",
    setFaviconColorToTitlebar
  );
  gBrowser.tabContainer.addEventListener("TabClose", setFaviconColorToTitlebar);
  gBrowser.tabContainer.addEventListener("TabOpen", setFaviconColorToTitlebar);
}

function disableFaviconColorToTitlebar() {
  let elems = document.querySelectorAll(".floorp-toolbar-bgcolor");
  for (let i = 0; i < elems.length; i++) {
    elems[i].remove();
  }

  gBrowser.tabContainer.removeEventListener(
    "TabSelect",
    setFaviconColorToTitlebar
  );
  gBrowser.tabContainer.removeEventListener(
    "TabMove",
    setFaviconColorToTitlebar
  );
  gBrowser.tabContainer.removeEventListener(
    "TabAttrModified",
    setFaviconColorToTitlebar
  );
  gBrowser.tabContainer.removeEventListener(
    "TabClose",
    setFaviconColorToTitlebar
  );
  gBrowser.tabContainer.removeEventListener(
    "TabOpen",
    setFaviconColorToTitlebar
  );
}

if (Services.prefs.getBoolPref("floorp.titlebar.favicon.color", true)) {
  window.setTimeout(() => {
    enableFaviconColorToTitlebar();
  }, 1000);

  Services.prefs.addObserver("floorp.titlebar.favicon.color", () => {
    if (Services.prefs.getBoolPref("floorp.titlebar.favicon.color", true)) {
      enableFaviconColorToTitlebar();
    } else {
      disableFaviconColorToTitlebar();
    }
  });
}
