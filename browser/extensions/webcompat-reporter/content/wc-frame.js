/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { utils: Cu } = Components;

const SCREENSHOT_MESSAGE = "WebCompat:SendScreenshot";

addMessageListener(SCREENSHOT_MESSAGE, function handleMessage(message) {
  removeMessageListener(SCREENSHOT_MESSAGE, handleMessage);
  // postMessage the screenshot blob from a content Sandbox so message event.origin
  // is what we expect on the client-side (i.e., https://webcompat.com)
  try {
    let sb = new Cu.Sandbox(content.document.nodePrincipal);
    sb.win = content;
    sb.screenshotBlob = Cu.cloneInto(message.data.screenshot, content);
    sb.wcOrigin = Cu.cloneInto(message.data.origin, content);
    Cu.evalInSandbox("win.postMessage(screenshotBlob, wcOrigin);", sb);
    Cu.nukeSandbox(sb);
  } catch (ex) {
    Cu.reportError(`WebCompatReporter: sending a screenshot failed: ${ex}`);
  }
});
