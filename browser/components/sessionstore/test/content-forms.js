/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

/**
 * This frame script is only loaded for sessionstore mochitests. It contains
 * a bunch of utility functions used to test form data collection and
 * restoration in remote browsers.
 */

function queryElement(data) {
  let frame = content;
  if (data.hasOwnProperty("frame")) {
    frame = content.frames[data.frame];
  }

  let doc = frame.document;

  if (data.hasOwnProperty("id")) {
    return doc.getElementById(data.id);
  }

  if (data.hasOwnProperty("selector")) {
    return doc.querySelector(data.selector);
  }

  if (data.hasOwnProperty("xpath")) {
    let xptype = doc.defaultView.XPathResult.FIRST_ORDERED_NODE_TYPE;
    return doc.evaluate(data.xpath, doc, null, xptype, null).singleNodeValue;
  }

  throw new Error("couldn't query element");
}

function dispatchUIEvent(input, type) {
  let event = input.ownerDocument.createEvent("UIEvents");
  event.initUIEvent(type, true, true, input.ownerGlobal, 0);
  input.dispatchEvent(event);
}

function defineListener(type, cb) {
  addMessageListener("ss-test:" + type, function({data}) {
    sendAsyncMessage("ss-test:" + type, cb(data));
  });
}

defineListener("getInnerHTML", function(data) {
  return queryElement(data).innerHTML;
});

defineListener("getTextContent", function(data) {
  return queryElement(data).textContent;
});

defineListener("getInputValue", function(data) {
  return queryElement(data).value;
});

defineListener("setInputValue", function(data) {
  let input = queryElement(data);
  input.value = data.value;
  dispatchUIEvent(input, "input");
});

defineListener("getInputChecked", function(data) {
  return queryElement(data).checked;
});

defineListener("setInputChecked", function(data) {
  let input = queryElement(data);
  input.checked = data.checked;
  dispatchUIEvent(input, "input");
});

defineListener("getSelectedIndex", function(data) {
  return queryElement(data).selectedIndex;
});

defineListener("setSelectedIndex", function(data) {
  let input = queryElement(data);
  input.selectedIndex = data.index;
  dispatchUIEvent(input, "input");
});

defineListener("getMultipleSelected", function(data) {
  let input = queryElement(data);
  return Array.from(input.options, (opt, idx) => idx)
              .filter(idx => input.options[idx].selected);
});

defineListener("setMultipleSelected", function(data) {
  let input = queryElement(data);
  Array.prototype.forEach.call(input.options, (opt, idx) => opt.selected = data.indices.indexOf(idx) > -1);
  dispatchUIEvent(input, "input");
});

defineListener("getFileNameArray", function(data) {
  return queryElement(data).mozGetFileNameArray();
});

defineListener("setFileNameArray", function(data) {
  let input = queryElement(data);
  input.mozSetFileNameArray(data.names, data.names.length);
  dispatchUIEvent(input, "input");
});

defineListener("setFormElementValues", function(data) {
  for (let elem of content.document.forms[0].elements) {
    elem.value = data.value;
    dispatchUIEvent(elem, "input");
  }
});
