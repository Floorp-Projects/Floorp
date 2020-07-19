/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// based on https://github.com/gorhill/uBlock/blob/8a1a8b103f56e4fcef1264e02dfd718a29bda006/src/web_accessible_resources/google-analytics_analytics.js

"use strict";

if (!window[window.GoogleAnalyticsObject || "ga"]) {
  function ga() {
    const len = arguments.length;
    if (!len) {
      return;
    }
    const args = Array.from(arguments);
    let fn;
    let a = args[len - 1];
    if (a instanceof Object && a.hitCallback instanceof Function) {
      fn = a.hitCallback;
    } else {
      const pos = args.indexOf("hitCallback");
      if (pos !== -1 && args[pos + 1] instanceof Function) {
        fn = args[pos + 1];
      }
    }
    if (!(fn instanceof Function)) {
      return;
    }
    try {
      fn();
    } catch (_) {}
  }
  ga.create = () => {};
  ga.getByName = () => null;
  ga.getAll = () => [];
  ga.remove = () => {};
  ga.loaded = true;

  const gaName = window.GoogleAnalyticsObject || "ga";
  const gaQueue = window[gaName];
  window[gaName] = ga;

  try {
    window.dataLayer.hide.end();
  } catch (_) {}

  if (gaQueue instanceof Function && Array.isArray(gaQueue.q)) {
    for (const entry of gaQueue.q) {
      ga(...entry);
    }
  }
}
