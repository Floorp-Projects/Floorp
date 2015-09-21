    // For mochitests, we're more interested in testing the behavior of in-
    // content XBL bindings, so we set this pref to true. In reftests, we're
    // more interested in testing the behavior of XBL as it works in chrome,
    // so we want this pref to be false.
    branch.setBoolPref("dom.use_xbl_scopes_for_remote_xul", false);
    branch.setIntPref("gfx.color_management.mode", 2);
    branch.setBoolPref("gfx.color_management.force_srgb", true);
    branch.setBoolPref("browser.dom.window.dump.enabled", true);
    branch.setIntPref("ui.caretBlinkTime", -1);
    branch.setBoolPref("dom.send_after_paint_to_content", true);
    // no slow script dialogs
    branch.setIntPref("dom.max_script_run_time", 0);
    branch.setIntPref("dom.max_chrome_script_run_time", 0);
    branch.setIntPref("hangmonitor.timeout", 0);
    // Ensure autoplay is enabled for all platforms.
    branch.setBoolPref("media.autoplay.enabled", true);
    // Disable updates
    branch.setBoolPref("app.update.enabled", false);
    // Disable addon updates and prefetching so we don't leak them
    branch.setBoolPref("extensions.update.enabled", false);
    branch.setBoolPref("extensions.getAddons.cache.enabled", false);
    // Disable blocklist updates so we don't have them reported as leaks
    branch.setBoolPref("extensions.blocklist.enabled", false);
    // Make url-classifier updates so rare that they won't affect tests
    branch.setIntPref("urlclassifier.updateinterval", 172800);
    // Disable high-quality downscaling, since it makes reftests more difficult.
    branch.setBoolPref("image.high_quality_downscaling.enabled", false);
    // Disable the single-color optimization, since it can cause intermittent
    // oranges and it causes many of our tests to test a different code path
    // than the one that normal images on the web use.
    branch.setBoolPref("image.single-color-optimization.enabled", false);
    // Checking whether two files are the same is slow on Windows.
    // Setting this pref makes tests run much faster there.
    branch.setBoolPref("security.fileuri.strict_origin_policy", false);
    // Disable the thumbnailing service
    branch.setBoolPref("browser.pagethumbnails.capturing_disabled", true);
    // Since our tests are 800px wide, set the assume-designed-for width of all
    // pages to be 800px (instead of the default of 980px). This ensures that
    // in our 800px window we don't zoom out by default to try to fit the
    // assumed 980px content.
    branch.setIntPref("browser.viewport.desktopWidth", 800);
    // Disable the fade out (over time) of overlay scrollbars, since we
    // can't guarantee taking both reftest snapshots at the same point
    // during the fade.
    branch.setBoolPref("layout.testing.overlay-scrollbars.always-visible", true);
    // Disable interruptible reflow since (1) it's normally not going to
    // happen, but (2) it might happen if we somehow end up with both
    // pending user events and clock skew.  So to avoid having to change
    // MakeProgress to deal with waiting for interruptible reflows to
    // complete for a rare edge case, we just disable interruptible
    // reflow so that that rare edge case doesn't lead to reftest
    // failures.
    branch.setBoolPref("layout.interruptible-reflow.enabled", false);
    // Disable the auto-hide feature of touch caret to avoid potential
    // intermittent issues.
    branch.setIntPref("touchcaret.expiration.time", 0);

    // Tell the search service we are running in the US.  This also has the
    // desired side-effect of preventing our geoip lookup.
    branch.setBoolPref("browser.search.isUS", true);
    branch.setCharPref("browser.search.countryCode", "US");
    // Prevent the geoSpecificDefaults XHR by emptying the URL.
    branch.setCharPref("browser.search.geoSpecificDefaults.url", "");

    // Make sure SelfSupport doesn't hit the network.
    branch.setCharPref("browser.selfsupport.url", "https://%(server)s/selfsupport-dummy/");

    // Disable periodic updates of service workers.
    branch.setBoolPref("dom.serviceWorkers.periodic-updates.enabled", false);

    // Allow XUL and XBL files to be opened from file:// URIs
    branch.setBoolPref("dom.allow_XUL_XBL_for_file", true);
