/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["StartupRecorder"];

const Cm = Components.manager;
Cm.QueryInterface(Ci.nsIServiceManager);

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

let firstPaintNotification = "widget-first-paint";
// widget-first-paint fires much later than expected on Linux.
if (
  AppConstants.platform == "linux" ||
  Services.prefs.getBoolPref("browser.startup.preXulSkeletonUI", false)
) {
  firstPaintNotification = "xul-window-visible";
}

let win, canvas;
let paints = [];
let afterPaintListener = () => {
  let width, height;
  canvas.width = width = win.innerWidth;
  canvas.height = height = win.innerHeight;
  if (width < 1 || height < 1) {
    return;
  }
  let ctx = canvas.getContext("2d", { alpha: false, willReadFrequently: true });

  ctx.drawWindow(
    win,
    0,
    0,
    width,
    height,
    "white",
    ctx.DRAWWINDOW_DO_NOT_FLUSH |
      ctx.DRAWWINDOW_DRAW_VIEW |
      ctx.DRAWWINDOW_ASYNC_DECODE_IMAGES |
      ctx.DRAWWINDOW_USE_WIDGET_LAYERS
  );
  paints.push({
    data: ctx.getImageData(0, 0, width, height).data,
    width,
    height,
  });
};

/**
 * The StartupRecorder component observes notifications at various stages of
 * startup and records the set of JS components and modules that were already
 * loaded at each of these points.
 * The records are meant to be used by startup tests in
 * browser/base/content/test/performance
 * This component only exists in nightly and debug builds, it doesn't ship in
 * our release builds.
 */
function StartupRecorder() {
  this.wrappedJSObject = this;
  this.data = {
    images: {
      "image-drawing": new Set(),
      "image-loading": new Set(),
    },
    code: {},
    extras: {},
    prefStats: {},
  };
  this.done = new Promise(resolve => {
    this._resolve = resolve;
  });
}
StartupRecorder.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  record(name) {
    ChromeUtils.addProfilerMarker("startupRecorder:" + name);
    this.data.code[name] = {
      components: Cu.loadedComponents,
      modules: Cu.loadedModules,
      services: Object.keys(Cc).filter(c => {
        try {
          return Cm.isServiceInstantiatedByContractID(c, Ci.nsISupports);
        } catch (e) {
          return false;
        }
      }),
    };
    this.data.extras[name] = {
      hiddenWindowLoaded: Services.appShell.hasHiddenWindow,
    };
  },

  observe(subject, topic, data) {
    if (topic == "app-startup" || topic == "content-process-ready-for-script") {
      // Don't do anything in xpcshell.
      if (Services.appinfo.ID != "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}") {
        return;
      }

      if (
        !Services.prefs.getBoolPref("browser.startup.record", false) &&
        !Services.prefs.getBoolPref("browser.startup.recordImages", false)
      ) {
        this._resolve();
        this._resolve = null;
        return;
      }

      // We can't ensure our observer will be called first or last, so the list of
      // topics we observe here should avoid the topics used to trigger things
      // during startup (eg. the topics observed by BrowserGlue.jsm).
      let topics = [
        "profile-do-change", // This catches stuff loaded during app-startup
        "toplevel-window-ready", // Catches stuff from final-ui-startup
        firstPaintNotification,
        "sessionstore-windows-restored",
        "browser-startup-idle-tasks-finished",
      ];

      if (Services.prefs.getBoolPref("browser.startup.recordImages", false)) {
        // For code simplicify, recording images excludes the other startup
        // recorder behaviors, so we can observe only the image topics.
        topics = [
          "image-loading",
          "image-drawing",
          "browser-startup-idle-tasks-finished",
        ];
      }
      for (let t of topics) {
        Services.obs.addObserver(this, t);
      }
      return;
    }

    // We only care about the first paint notification for browser windows, and
    // not other types (for example, the gfx sanity test window)
    if (topic == firstPaintNotification) {
      // In the case we're handling xul-window-visible, we'll have been handed
      // an nsIAppWindow instead of an nsIDOMWindow.
      if (subject instanceof Ci.nsIAppWindow) {
        subject = subject
          .QueryInterface(Ci.nsIInterfaceRequestor)
          .getInterface(Ci.nsIDOMWindow);
      }

      if (
        subject.document.documentElement.getAttribute("windowtype") !=
        "navigator:browser"
      ) {
        return;
      }
    }

    if (topic == "image-drawing" || topic == "image-loading") {
      this.data.images[topic].add(data);
      return;
    }

    Services.obs.removeObserver(this, topic);

    if (topic == firstPaintNotification) {
      // Because of the check for navigator:browser we made earlier, we know
      // that if we got here, then the subject must be the first browser window.
      win = subject;
      canvas = win.document.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "canvas"
      );
      canvas.mozOpaque = true;
      afterPaintListener();
      win.addEventListener("MozAfterPaint", afterPaintListener);
    }

    if (topic == "sessionstore-windows-restored") {
      // We use idleDispatchToMainThread here to record the set of
      // loaded scripts after we are fully done with startup and ready
      // to react to user events.
      Services.tm.dispatchToMainThread(
        this.record.bind(this, "before handling user events")
      );
    } else if (topic == "browser-startup-idle-tasks-finished") {
      if (Services.prefs.getBoolPref("browser.startup.recordImages", false)) {
        Services.obs.removeObserver(this, "image-drawing");
        Services.obs.removeObserver(this, "image-loading");
        this._resolve();
        this._resolve = null;
        return;
      }

      this.record("before becoming idle");
      win.removeEventListener("MozAfterPaint", afterPaintListener);
      win = null;
      this.data.frames = paints;
      this.data.prefStats = {};
      if (AppConstants.DEBUG) {
        Services.prefs.readStats(
          (key, value) => (this.data.prefStats[key] = value)
        );
      }
      paints = null;

      let env = Cc["@mozilla.org/process/environment;1"].getService(
        Ci.nsIEnvironment
      );
      if (!env.exists("MOZ_PROFILER_STARTUP_PERFORMANCE_TEST")) {
        this._resolve();
        this._resolve = null;
        return;
      }

      Services.profiler.getProfileDataAsync().then(profileData => {
        this.data.profile = profileData;
        // There's no equivalent StartProfiler call in this file because the
        // profiler is started using the MOZ_PROFILER_STARTUP environment
        // variable in browser/base/content/test/performance/browser.ini
        Services.profiler.StopProfiler();

        this._resolve();
        this._resolve = null;
      });
    } else {
      const topicsToNames = {
        "profile-do-change": "before profile selection",
        "toplevel-window-ready": "before opening first browser window",
      };
      topicsToNames[firstPaintNotification] = "before first paint";
      this.record(topicsToNames[topic]);
    }
  },
};
