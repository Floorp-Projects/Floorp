/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* globals addMessageListener, sendAsyncMessage */

addMessageListener("Test:cssRules", function(msg) {
  let {num} = msg.data;
  let sheet = content.document.styleSheets[num];
  let result = [];
  for (let i = 0; i < sheet.cssRules.length; ++i) {
    result.push(sheet.cssRules[i].cssText);
  }
  sendAsyncMessage("Test:cssRules", result);
});

/**
 * Get the property value from the computed style for an element.
 * @param {Object} data Expects a data object with the following properties
 * - {String} selector: The selector used to obtain the element.
 * - {String} pseudo: pseudo id to query, or null.
 * - {String} name: name of the property
 * @return {String} The value, if found, null otherwise
 */
addMessageListener("Test:GetComputedStylePropertyValue", function(msg) {
  let {selector, pseudo, name} = msg.data;
  let element = content.document.querySelector(selector);
  let style = content.document.defaultView.getComputedStyle(element, pseudo);
  let value = style.getPropertyValue(name);
  sendAsyncMessage("Test:GetComputedStylePropertyValue", value);
});

addMessageListener("Test:GetStyleContent", function() {
  sendAsyncMessage("Test:GetStyleContent",
                   content.document.querySelector("style").textContent);
});
