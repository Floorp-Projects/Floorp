/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals gNetwork, NetMonitorController */

"use strict";

const { Task } = require("devtools/shared/task");
const { formDataURI } = require("../request-utils");
const { WEBCONSOLE_L10N } = require("../l10n");
const { setImageTooltip,
        getImageDimensions } = require("devtools/client/shared/widgets/tooltip/ImageTooltipHelper");

// px
const REQUESTS_TOOLTIP_IMAGE_MAX_DIM = 400;
// px
const REQUESTS_TOOLTIP_STACK_TRACE_WIDTH = 600;

const HTML_NS = "http://www.w3.org/1999/xhtml";

const setTooltipImageContent = Task.async(function* (tooltip, itemEl, requestItem) {
  let { mimeType, text, encoding } = requestItem.responseContent.content;

  if (!mimeType || !mimeType.includes("image/")) {
    return false;
  }

  let string = yield gNetwork.getString(text);
  let src = formDataURI(mimeType, encoding, string);
  let maxDim = REQUESTS_TOOLTIP_IMAGE_MAX_DIM;
  let { naturalWidth, naturalHeight } = yield getImageDimensions(tooltip.doc, src);
  let options = { maxDim, naturalWidth, naturalHeight };
  setImageTooltip(tooltip, tooltip.doc, src, options);

  return itemEl.querySelector(".requests-menu-icon");
});

const setTooltipStackTraceContent = Task.async(function* (tooltip, requestItem) {
  let {stacktrace} = requestItem.cause;

  if (!stacktrace || stacktrace.length == 0) {
    return false;
  }

  let doc = tooltip.doc;
  let el = doc.createElementNS(HTML_NS, "div");
  el.className = "stack-trace-tooltip devtools-monospace";

  for (let f of stacktrace) {
    let { functionName, filename, lineNumber, columnNumber, asyncCause } = f;

    if (asyncCause) {
      // if there is asyncCause, append a "divider" row into the trace
      let asyncFrameEl = doc.createElementNS(HTML_NS, "div");
      asyncFrameEl.className = "stack-frame stack-frame-async";
      asyncFrameEl.textContent =
        WEBCONSOLE_L10N.getFormatStr("stacktrace.asyncStack", asyncCause);
      el.appendChild(asyncFrameEl);
    }

    // Parse a source name in format "url -> url"
    let sourceUrl = filename.split(" -> ").pop();

    let frameEl = doc.createElementNS(HTML_NS, "div");
    frameEl.className = "stack-frame stack-frame-call";

    let funcEl = doc.createElementNS(HTML_NS, "span");
    funcEl.className = "stack-frame-function-name";
    funcEl.textContent =
      functionName || WEBCONSOLE_L10N.getStr("stacktrace.anonymousFunction");
    frameEl.appendChild(funcEl);

    let sourceEl = doc.createElementNS(HTML_NS, "span");
    sourceEl.className = "stack-frame-source-name";
    frameEl.appendChild(sourceEl);

    let sourceInnerEl = doc.createElementNS(HTML_NS, "span");
    sourceInnerEl.className = "stack-frame-source-name-inner";
    sourceEl.appendChild(sourceInnerEl);

    sourceInnerEl.textContent = sourceUrl;
    sourceInnerEl.title = sourceUrl;

    let lineEl = doc.createElementNS(HTML_NS, "span");
    lineEl.className = "stack-frame-line";
    lineEl.textContent = `:${lineNumber}:${columnNumber}`;
    sourceInnerEl.appendChild(lineEl);

    frameEl.addEventListener("click", () => {
      // hide the tooltip immediately, not after delay
      tooltip.hide();
      NetMonitorController.viewSourceInDebugger(filename, lineNumber);
    }, false);

    el.appendChild(frameEl);
  }

  tooltip.setContent(el, {width: REQUESTS_TOOLTIP_STACK_TRACE_WIDTH});

  return true;
});

module.exports = {
  setTooltipImageContent,
  setTooltipStackTraceContent,
};
