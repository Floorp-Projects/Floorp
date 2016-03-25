pref("loop.enabled", true);
pref("loop.remote.autostart", true);
pref("loop.server", "https://loop.services.mozilla.com/v0");
pref("loop.linkClicker.url", "https://hello.firefox.com/");
pref("loop.gettingStarted.latestFTUVersion", 1);
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
pref("loop.feedback.formURL", "https://www.surveygizmo.com/s3/2651383/Firefox-Hello-Product-Survey-II");
pref("loop.feedback.manualFormURL", "https://www.mozilla.org/firefox/hello/feedbacksurvey/");
pref("loop.logDomains", false);
pref("loop.mau.openPanel", 0);
pref("loop.mau.openConversation", 0);
pref("loop.mau.roomOpen", 0);
pref("loop.mau.roomShare", 0);
pref("loop.mau.roomDelete", 0);
#ifdef DEBUG
pref("loop.CSP", "default-src 'self' about: file: chrome: http://localhost:*; img-src * data:; font-src 'none'; connect-src wss://*.tokbox.com https://*.opentok.com https://*.tokbox.com wss://*.mozilla.com https://*.mozilla.org wss://*.mozaws.net http://localhost:* ws://localhost:*; media-src blob:");
#else
pref("loop.CSP", "default-src 'self' about: file: chrome:; img-src * data:; font-src 'none'; connect-src wss://*.tokbox.com https://*.opentok.com https://*.tokbox.com wss://*.mozilla.com https://*.mozilla.org wss://*.mozaws.net; media-src blob:");
#endif
pref("loop.fxa_oauth.tokendata", "");
pref("loop.fxa_oauth.profile", "");
pref("loop.support_url", "https://support.mozilla.org/1/firefox/%VERSION%/%OS%/%LOCALE%/cobrowsing");
pref("loop.facebook.enabled", true);
pref("loop.facebook.appId", "1519239075036718");
pref("loop.facebook.shareUrl", "https://www.facebook.com/dialog/send?app_id=%APP_ID%&link=%ROOM_URL%&redirect_uri=%REDIRECT_URI%");
pref("loop.facebook.fallbackUrl", "https://hello.firefox.com/");
pref("loop.conversationPopOut.enabled", true);
