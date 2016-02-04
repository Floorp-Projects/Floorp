pref("loop.enabled", true);
pref("loop.server", "https://loop.services.mozilla.com/v0");
pref("loop.linkClicker.url", "https://hello.firefox.com/");
pref("loop.gettingStarted.latestFTUVersion", 1);
pref("loop.facebook.shareUrl", "https://www.facebook.com/sharer/sharer.php?u=%ROOM_URL%");
pref("loop.gettingStarted.url", "https://www.mozilla.org/%LOCALE%/firefox/%VERSION%/hello/start/");
pref("loop.gettingStarted.resumeOnFirstJoin", false);
pref("loop.legal.ToS_url", "https://www.mozilla.org/about/legal/terms/firefox-hello/");
pref("loop.legal.privacy_url", "https://www.mozilla.org/privacy/firefox-hello/");
pref("loop.do_not_disturb", false);
pref("loop.retry_delay.start", 60000);
pref("loop.retry_delay.limit", 300000);
pref("loop.ping.interval", 1800000);
pref("loop.ping.timeout", 10000);
pref("loop.debug.loglevel", "Error");
pref("loop.debug.dispatcher", false);
pref("loop.debug.sdk", false);
pref("loop.debug.twoWayMediaTelemetry", false);
pref("loop.feedback.dateLastSeenSec", 0);
pref("loop.feedback.periodSec", 15770000); // 6 months.
pref("loop.feedback.formURL", "https://www.mozilla.org/firefox/hello/npssurvey/");
pref("loop.feedback.manualFormURL", "https://www.mozilla.org/firefox/hello/feedbacksurvey/");
#ifdef DEBUG
pref("loop.CSP", "default-src 'self' about: file: chrome: http://localhost:*; img-src * data:; font-src 'none'; connect-src wss://*.tokbox.com https://*.opentok.com https://*.tokbox.com wss://*.mozilla.com https://*.mozilla.org wss://*.mozaws.net http://localhost:* ws://localhost:*; media-src blob:");
#else
pref("loop.CSP", "default-src 'self' about: file: chrome:; img-src * data:; font-src 'none'; connect-src wss://*.tokbox.com https://*.opentok.com https://*.tokbox.com wss://*.mozilla.com https://*.mozilla.org wss://*.mozaws.net; media-src blob:");
#endif
pref("loop.fxa_oauth.tokendata", "");
pref("loop.fxa_oauth.profile", "");
pref("loop.support_url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/cobrowsing");
#ifdef LOOP_BETA
pref("loop.facebook.enabled", true);
#else
pref("loop.facebook.enabled", false);
#endif
