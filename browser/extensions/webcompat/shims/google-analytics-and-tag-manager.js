/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713687 - Shim Google Analytics and Tag Manager
 *
 * Sites often rely on the Google Analytics window object and will
 * break if it fails to load or is blocked. This shim works around
 * such breakage.
 *
 * Sites also often use the Google Optimizer (asynchide) code snippet,
 * only for it to cause multi-second delays if Google Analytics does
 * not load. This shim also avoids such delays.
 *
 * They also rely on Google Tag Manager, which often goes hand-in-
 * hand with Analytics, but is not always blocked by anti-tracking
 * lists. Handling both in the same shim handles both cases.
 */

if (window[window.GoogleAnalyticsObject || "ga"]?.loaded === undefined) {
  const DEFAULT_TRACKER_NAME = "t0";

  const trackers = new Map();

  const run = function(fn, ...args) {
    if (typeof fn === "function") {
      try {
        fn(...args);
      } catch (e) {
        console.error(e);
      }
    }
  };

  const create = (id, cookie, name, opts) => {
    id = id || opts?.trackerId;
    if (!id) {
      return undefined;
    }
    cookie = cookie || opts?.cookieDomain || "_ga";
    name = name || opts?.name || DEFAULT_TRACKER_NAME;
    if (!trackers.has(name)) {
      let props;
      try {
        props = new Map(Object.entries(opts));
      } catch (_) {
        props = new Map();
      }
      trackers.set(name, {
        get(p) {
          if (p === "name") {
            return name;
          } else if (p === "trackingId") {
            return id;
          } else if (p === "cookieDomain") {
            return cookie;
          }
          return props.get(p);
        },
        ma() {},
        requireSync() {},
        send() {},
        set(p, v) {
          if (typeof p !== "object") {
            p = Object.fromEntries([[p, v]]);
          }
          for (const k in p) {
            props.set(k, p[k]);
            if (k === "hitCallback") {
              run(p[k]);
            }
          }
        },
      });
    }
    return trackers.get(name);
  };

  const cmdRE = /((?<name>.*?)\.)?((?<plugin>.*?):)?(?<method>.*)/;

  function ga(cmd, ...args) {
    if (arguments.length === 1 && typeof cmd === "function") {
      run(cmd, trackers.get(DEFAULT_TRACKER_NAME));
      return undefined;
    }

    if (typeof cmd !== "string") {
      return undefined;
    }

    const groups = cmdRE.exec(cmd)?.groups;
    if (!groups) {
      console.error("Could not parse GA command", cmd);
      return undefined;
    }

    let { name, plugin, method } = groups;

    if (plugin) {
      return undefined;
    }

    if (cmd === "set") {
      trackers.get(name)?.set(args[0], args[1]);
    }

    if (method === "remove") {
      trackers.delete(name);
      return undefined;
    }

    if (cmd === "send") {
      run(args.at(-1)?.hitCallback);
      return undefined;
    }

    if (method === "create") {
      let id, cookie, fields;
      for (const param of args.slice(0, 4)) {
        if (typeof param === "object") {
          fields = param;
          break;
        }
        if (id === undefined) {
          id = param;
        } else if (cookie === undefined) {
          cookie = param;
        } else {
          name = param;
        }
      }
      return create(id, cookie, name, fields);
    }

    return undefined;
  }

  Object.assign(ga, {
    create: (a, b, c, d) => ga("create", a, b, c, d),
    getAll: () => Array.from(trackers.values()),
    getByName: name => trackers.get(name),
    loaded: true,
    remove: t => ga("remove", t),
  });

  // Process any GA command queue the site pre-declares (bug 1736850)
  const q = window[window.GoogleAnalyticsObject || "ga"]?.q;
  window[window.GoogleAnalyticsObject || "ga"] = ga;

  if (Array.isArray(q)) {
    const push = o => {
      ga(...o);
      return true;
    };
    q.push = push;
    q.forEach(o => push(o));
  }

  // Also process the Google Tag Manager dataLayer (bug 1713688)
  const dl = window.dataLayer;

  if (Array.isArray(dl)) {
    const oldPush = dl.push;
    const push = function(o) {
      if (oldPush) {
        return oldPush.apply(dl, arguments);
      }
      setTimeout(() => run(o?.eventCallback), 1);
      return true;
    };
    dl.push = push;
    dl.forEach(o => push(o));
  }

  // Run dataLayer.hide.end to handle asynchide (bug 1628151)
  run(window.dataLayer?.hide?.end);
}

if (!window?.gaplugins?.Linker) {
  window.gaplugins = window.gaplugins || {};
  window.gaplugins.Linker = class {
    autoLink() {}
    decorate(url) {
      return url;
    }
    passthrough() {}
  };
}
