/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* global addMessageListener, sendAsyncMessage, content */
"use strict";
const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const Services = require("Services");

addMessageListener("devtools:test:console", function({ data }) {
  const { method, args, id } = data;
  content.console[method].apply(content.console, args);
  sendAsyncMessage("devtools:test:console:response", { id });
});

addMessageListener("devtools:test:profiler", function({ data }) {
  const { method, args, id } = data;
  const result = Services.profiler[method](...args);
  sendAsyncMessage("devtools:test:profiler:response", {
    data: result,
    id: id,
  });
});

// To eval in content, look at `evalInDebuggee` in the shared-head.js.
addMessageListener("devtools:test:eval", function({ data }) {
  sendAsyncMessage("devtools:test:eval:response", {
    value: content.eval(data.script),
    id: data.id,
  });
});
