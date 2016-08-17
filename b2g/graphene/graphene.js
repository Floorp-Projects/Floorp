// See http://dxr.mozilla.org/mozilla-central/source/dom/webidl/KeyEvent.webidl
// for keyCode values.
// Default value is F5
pref("b2g.reload_key", '{ "key": 116, "shift": false, "ctrl": false, "alt": false, "meta": false }');

#ifdef MOZ_HORIZON
pref("b2g.default.start_manifest_url", "https://mozvr.github.io/horizon/web/manifest.webapp");
pref("dom.vr.enabled", true);
pref("dom.ipc.tabs.disabled", true);
#else
pref("b2g.default.start_manifest_url", "https://mozilla.github.io/browser.html/manifest.webapp");
pref("dom.ipc.tabs.disabled", false);
#endif

pref("javascript.options.discardSystemSource", false);
pref("browser.dom.window.dump.enabled", true);
pref("browser.ignoreNativeFrameTextSelection", false);
pref("dom.meta-viewport.enabled", false);
pref("full-screen-api.ignore-widgets", false);
pref("image.high_quality_downscaling.enabled", true);
pref("dom.w3c_touch_events.enabled", 0);
pref("font.size.inflation.minTwips", 0);
pref("browser.enable_click_image_resizing", true);
pref("layout.css.scroll-snap.enabled", true);
pref("dom.mozInputMethod.enabled", false);
pref("browser.autofocus", true);
pref("layers.async-pan-zoom.enabled", false);
pref("network.predictor.enabled", true);

// No AccessibleCaret
pref("layout.accessiblecaret.enabled", false);

// To be removed once bug 942756 is fixed.
pref("devtools.debugger.unix-domain-socket", "6000");

pref("devtools.debugger.forbid-certified-apps", false);
pref("devtools.debugger.prompt-connection", false);

// Update url.
pref("app.update.url", "https://aus4.mozilla.org/update/3/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml");

pref("b2g.nativeWindowGeometry.width", 700);
pref("b2g.nativeWindowGeometry.height", 600);
pref("b2g.nativeWindowGeometry.screenX", -1); // center
pref("b2g.nativeWindowGeometry.screenY", -1); // center
pref("b2g.nativeWindowGeometry.fullscreen", false);

pref("media.useAudioChannelService", false);

#ifdef ENABLE_MARIONETTE
pref("b2g.is_mulet", true);
#endif

// Most DevTools prefs are set from the shared file
// devtools/client/preferences/devtools.js, but this one is currently set
// per-app or per-channel.
// Number of usages of the web console or scratchpad. If this is less than 5,
// then pasting code into the web console or scratchpad is disabled
pref("devtools.selfxss.count", 5);
