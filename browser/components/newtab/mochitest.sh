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
  && ./mach test browser_parsable_css \
  && ./mach lint -l codespell browser/components/newtab \
  && ./mach test browser/components/newtab/test/browser --headless \
  && ./mach test browser/components/newtab/test/xpcshell \
  && ./mach test browser/components/preferences/in-content/tests/browser_hometab_restore_defaults.js --headless \
  && ./mach test browser/components/preferences/in-content/tests/browser_newtab_menu.js --headless \
  && ./mach test browser/components/enterprisepolicies/tests/browser/browser_policy_set_homepage.js --headless \
  && ./mach test browser/components/preferences/in-content/tests/browser_search_subdialogs_within_preferences_1.js --headless
