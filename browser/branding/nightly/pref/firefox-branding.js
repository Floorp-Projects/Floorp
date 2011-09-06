pref("startup.homepage_override_url","http://www.mozilla.org/projects/%APP%/%VERSION%/whatsnew/");
pref("startup.homepage_welcome_url","http://www.mozilla.org/projects/%APP%/%VERSION%/firstrun/");
// The time interval between checks for a new version (in seconds)
pref("app.update.interval", 7200); // 2 hours
// The time interval between the downloading of mar file chunks in the
// background (in seconds)
pref("app.update.download.backgroundInterval", 60);
// URL user can browse to manually if for some reason all update installation
// attempts fail.
pref("app.update.url.manual", "http://nightly.mozilla.org/");
// A default value for the "More information about this update" link
// supplied in the "An update is available" page of the update wizard. 
pref("app.update.url.details", "http://www.mozilla.org/projects/%APP%/");

// Release notes and vendor URLs
pref("app.releaseNotesURL", "http://www.mozilla.org/projects/%APP%/%VERSION%/releasenotes/");
pref("app.vendorURL", "http://www.mozilla.org/projects/%APP%/");

// Search codes belong only in builds with official branding
pref("browser.search.param.yahoo-fr", "");
pref("browser.search.param.yahoo-fr-cjkt", ""); // now unused
pref("browser.search.param.yahoo-fr-ja", "");
pref("browser.search.param.yahoo-f-CN", "");
