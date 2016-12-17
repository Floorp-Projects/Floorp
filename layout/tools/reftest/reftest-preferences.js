// For mochitests, we're more interested in testing the behavior of in-
// content XBL bindings, so we set this pref to true. In reftests, we're
// more interested in testing the behavior of XBL as it works in chrome,
// so we want this pref to be false.
user_pref("dom.use_xbl_scopes_for_remote_xul", false);
user_pref("gfx.color_management.mode", 2);
user_pref("gfx.color_management.force_srgb", true);
user_pref("gfx.logging.level", 1);
user_pref("browser.dom.window.dump.enabled", true);
user_pref("ui.caretBlinkTime", -1);
user_pref("dom.send_after_paint_to_content", true);
// no slow script dialogs
user_pref("dom.max_script_run_time", 0);
user_pref("dom.max_chrome_script_run_time", 0);
user_pref("hangmonitor.timeout", 0);
// Ensure autoplay is enabled for all platforms.
user_pref("media.autoplay.enabled", true);
// Disable updates
user_pref("app.update.enabled", false);
user_pref("app.update.staging.enabled", false);
user_pref("app.update.url.android", "");
// Disable addon updates and prefetching so we don't leak them
user_pref("extensions.update.enabled", false);
user_pref("extensions.systemAddon.update.url", "http://localhost/dummy-system-addons.xml");
user_pref("extensions.getAddons.cache.enabled", false);
// Disable blocklist updates so we don't have them reported as leaks
user_pref("extensions.blocklist.enabled", false);
// Make url-classifier updates so rare that they won't affect tests
user_pref("urlclassifier.updateinterval", 172800);
// Disable downscale-during-decode, since it makes reftests more difficult.
user_pref("image.downscale-during-decode.enabled", false);
// Checking whether two files are the same is slow on Windows.
// Setting this pref makes tests run much faster there. Reftests also
// rely on this to load downloadable fonts (which are restricted to same
// origin policy by default) from outside their directory.
user_pref("security.fileuri.strict_origin_policy", false);
// Disable the thumbnailing service
user_pref("browser.pagethumbnails.capturing_disabled", true);
// Since our tests are 800px wide, set the assume-designed-for width of all
// pages to be 800px (instead of the default of 980px). This ensures that
// in our 800px window we don't zoom out by default to try to fit the
// assumed 980px content.
user_pref("browser.viewport.desktopWidth", 800);
// Disable the fade out (over time) of overlay scrollbars, since we
// can't guarantee taking both reftest snapshots at the same point
// during the fade.
user_pref("layout.testing.overlay-scrollbars.always-visible", true);
// Disable interruptible reflow since (1) it's normally not going to
// happen, but (2) it might happen if we somehow end up with both
// pending user events and clock skew.  So to avoid having to change
// MakeProgress to deal with waiting for interruptible reflows to
// complete for a rare edge case, we just disable interruptible
// reflow so that that rare edge case doesn't lead to reftest
// failures.
user_pref("layout.interruptible-reflow.enabled", false);

// Tell the search service we are running in the US.  This also has the
// desired side-effect of preventing our geoip lookup.
user_pref("browser.search.isUS", true);
user_pref("browser.search.countryCode", "US");
user_pref("browser.search.geoSpecificDefaults", false);

// Make sure SelfSupport doesn't hit the network.
user_pref("browser.selfsupport.url", "https://localhost/selfsupport-dummy/");

// use about:blank, not browser.startup.homepage
user_pref("browser.startup.page", 0);

// Allow XUL and XBL files to be opened from file:// URIs
user_pref("dom.allow_XUL_XBL_for_file", true);

// Allow view-source URIs to be opened from URIs that share
// their protocol with the inner URI of the view-source URI
user_pref("security.view-source.reachable-from-inner-protocol", true);

// Ensure that telemetry is disabled, so we don't connect to the telemetry
// server in the middle of the tests.
user_pref("toolkit.telemetry.enabled", false);
user_pref("toolkit.telemetry.unified", false);
// Likewise for safebrowsing.
user_pref("browser.safebrowsing.phishing.enabled", false);
user_pref("browser.safebrowsing.malware.enabled", false);
user_pref("browser.safebrowsing.forbiddenURIs.enabled", false);
user_pref("browser.safebrowsing.blockedURIs.enabled", false);
// Likewise for tracking protection.
user_pref("privacy.trackingprotection.enabled", false);
user_pref("privacy.trackingprotection.pbmode.enabled", false);
// And for snippets.
user_pref("browser.snippets.enabled", false);
user_pref("browser.snippets.syncPromo.enabled", false);
user_pref("browser.snippets.firstrunHomepage.enabled", false);
// And for useragent updates.
user_pref("general.useragent.updates.enabled", false);
// And for webapp updates.  Yes, it is supposed to be an integer.
user_pref("browser.webapps.checkForUpdates", 0);
// And for about:newtab content fetch and pings.
user_pref("browser.newtabpage.directory.source", "data:application/json,{\"reftest\":1}");
user_pref("browser.newtabpage.directory.ping", "");
// Only allow add-ons from the profile and app and allow foreign
// injection
user_pref("extensions.enabledScopes", 5);
user_pref("extensions.autoDisableScopes", 0);
// Allow unsigned add-ons
user_pref("xpinstall.signatures.required", false);

// Don't use auto-enabled e10s
user_pref("browser.tabs.remote.autostart.1", false);
user_pref("browser.tabs.remote.autostart.2", false);

user_pref("startup.homepage_welcome_url", "");
user_pref("startup.homepage_welcome_url.additional", "");
user_pref("startup.homepage_override_url", "");

user_pref("media.gmp-manager.url.override", "http://localhost/dummy-gmp-manager.xml");
user_pref("media.gmp-manager.updateEnabled", false);

// A fake bool pref for "@supports -moz-bool-pref" sanify test.
user_pref("testing.supports.moz-bool-pref", true);

// Reftests load a lot of URLs very quickly. This puts avoidable and
// unnecessary I/O pressure on the Places DB (measured to be in the
// gigabytes).
user_pref("places.history.enabled", false);

// For Firefox 52 only, ESR will support non-Flash plugins while release will
// not, so we keep testing the non-Flash pathways
user_pref("plugin.load_flash_only", false);

user_pref("media.openUnsupportedTypeWithExternalApp", false);
