/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-env node */
"use strict";

/* global module */
async function test(context, commands) {
  await context.selenium.driver.setContext("chrome");
  let elementData = await context.selenium.driver.executeAsyncScript(
    function () {
      let callback = arguments[arguments.length - 1];
      (async function () {
        let lightDOM = document.querySelectorAll("*");
        let elementsWithoutIDs = {};
        let idElements = [];
        let lightDOMDetails = { idElements, elementsWithoutIDs };
        lightDOM.forEach(n => {
          if (n.id) {
            idElements.push(n.id);
          } else {
            if (!elementsWithoutIDs.hasOwnProperty(n.localName)) {
              elementsWithoutIDs[n.localName] = 0;
            }
            elementsWithoutIDs[n.localName]++;
          }
        });
        let lightDOMCount = lightDOM.length;

        // Recursively explore shadow DOM:
        function getShadowElements(root) {
          let allElems = Array.from(root.querySelectorAll("*"));
          let shadowRoots = allElems.map(n => n.openOrClosedShadowRoot);
          for (let innerRoot of shadowRoots) {
            if (innerRoot) {
              allElems.push(getShadowElements(innerRoot));
            }
          }
          return allElems;
        }
        let totalDOMCount = Array.from(lightDOM, node => {
          if (node.openOrClosedShadowRoot) {
            return [node].concat(
              getShadowElements(node.openOrClosedShadowRoot)
            );
          }
          return node;
        }).flat().length;
        let panelMenuCount = document.querySelectorAll(
          "panel,menupopup,popup,popupnotification"
        ).length;
        return {
          panelMenuCount,
          lightDOMCount,
          totalDOMCount,
          lightDOMDetails,
        };
      })().then(callback);
    }
  );
  let { lightDOMDetails } = elementData;
  delete elementData.lightDOMDetails;
  lightDOMDetails.idElements.sort();
  for (let id of lightDOMDetails.idElements) {
    console.log(id);
  }
  console.log("Elements without ids:");
  for (let [localName, count] of Object.entries(
    lightDOMDetails.elementsWithoutIDs
  )) {
    console.log(count.toString().padStart(4) + " " + localName);
  }
  console.log(elementData);
  await context.selenium.driver.setContext("content");
  await commands.measure.start("data:text/html,BrowserDOM");
  commands.measure.addObject(elementData);
}

module.exports = {
  test,
  owner: "Browser Front-end team",
  name: "Dom-size",
  description: "Measures the size of the DOM",
  supportedBrowsers: ["Desktop"],
  supportedPlatforms: ["Windows", "Linux", "macOS"],
};
