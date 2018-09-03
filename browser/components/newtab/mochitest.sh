#!/bin/bash

export SHELL=/bin/bash
# Display required for `browser_parsable_css` tests
export DISPLAY=:99.0
/sbin/start-stop-daemon --start --quiet --pidfile /tmp/custom_xvfb_99.pid --make-pidfile --background --exec /usr/bin/Xvfb -- :99 -ac -screen 0 1280x1024x16 -extension RANDR

# Pull latest m-c and update tip
cd /mozilla-central && hg pull && hg update -C

# Build Activity Stream and copy the output to m-c
cd /activity-stream && npm install . && npm run buildmc

# Build latest m-c with Activity Stream changes
cd /mozilla-central && ./mach build \
  && ./mach lint -l codespell browser/components/newtab \
  && ./mach test --log-tbpl test_run_log \
  browser_parsable_css \
  browser/components/newtab \
  browser/components/preferences/in-content/tests/browser_hometab_restore_defaults.js \
  browser/components/preferences/in-content/tests/browser_newtab_menu.js \
  browser/components/enterprisepolicies/tests/browser/browser_policy_set_homepage.js \
  browser/components/preferences/in-content/tests/browser_search_subdialogs_within_preferences_1.js \
  browser/extensions/onboarding/test/browser \
  && ! grep -q TEST-UNEXPECTED test_run_log
