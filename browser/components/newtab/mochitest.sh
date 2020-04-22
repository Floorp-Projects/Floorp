#!/bin/bash

export SHELL=/bin/bash
export TASKCLUSTER_ROOT_URL="https://taskcluster.net"
# Display required for `browser_parsable_css` tests
export DISPLAY=:99.0
# Required to support the unicode in the output
export LC_ALL=C.UTF-8
/sbin/start-stop-daemon --start --quiet --pidfile /tmp/custom_xvfb_99.pid --make-pidfile --background --exec /usr/bin/Xvfb -- :99 -ac -screen 0 1280x1024x16 -extension RANDR

# Pull latest m-c and update tip
cd /mozilla-central && hg pull && hg update -C

# Build Activity Stream and copy the output to m-c
cd /activity-stream && npm install . && npm run buildmc

# Build latest m-c with Activity Stream changes
cd /mozilla-central && rm -rf ./objdir-frontend && ./mach build \
  && ./mach lint browser/components/newtab \
  && ./mach lint -l codespell browser/locales/en-US/browser/newtab \
  && ./mach test browser/components/newtab/test/browser --headless \
  && ./mach test browser/components/newtab/test/xpcshell \
  && ./mach test --log-tbpl test_run_log \
    browser/base/content/test/about/browser_aboutHome_search_telemetry.js \
    browser/base/content/test/static/browser_parsable_css.js \
    browser/base/content/test/tabs/browser_new_tab_in_privileged_process_pref.js \
    browser/components/enterprisepolicies/tests/browser/browser_policy_set_homepage.js \
    browser/components/extensions/test/browser/browser_ext_topSites.js \
    browser/components/preferences/tests/browser_hometab_restore_defaults.js \
    browser/components/preferences/tests/browser_newtab_menu.js \
    browser/components/preferences/tests/browser_search_subdialogs_within_preferences_1.js \
    browser/components/search/test/browser/browser_google_behavior.js \
    browser/modules/test/browser/browser_UsageTelemetry_content.js \
  && ! grep -q TEST-UNEXPECTED test_run_log \
  && RUN_FIND_DUPES=1 ./mach package \
  && ./mach test --appname=dist all_files_referenced --headless
