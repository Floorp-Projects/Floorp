#!/bin/sh
# Run from topsrcdir, no args

if [ "$1" = "--help" ]; then
  echo "Usage: ./run-all-loop-tests.sh [options]"
  echo "    --skip-e10s  Skips the e10s tests"
  exit 0;
fi

# Causes script to abort immediately if error code is not checked.
set -e

# Main tests

LOOPDIR=browser/extensions/loop

./mach xpcshell-test ${LOOPDIR}/
./mach marionette-test ${LOOPDIR}/manifest.ini

# The browser_parsable_css.js can fail if we add some css that isn't parsable.
#
# The check to make sure that the media devices can be used in Loop without
# prompting is in browser_devices_get_user_media_about_urls.js. It's possible
# to mess this up with CSP handling, and probably other changes, too.

# Currently disabled due to Bug 1225832 - New Loop architecture is not compatible with test.
#  browser/components/uitour/test/browser_UITour_loop.js

TESTS="
  ${LOOPDIR}/chrome/test/mochitest
  browser/components/uitour/test/browser_UITour_loop_panel.js
  browser/base/content/test/general/browser_devices_get_user_media_about_urls.js
  browser/base/content/test/general/browser_parsable_css.js
"

# Due to bug 1209463, we need to split these up and run them individually to
# ensure we stop and report that there's an error.
for test in $TESTS
do
  ./mach mochitest --disable-e10s $test
  # UITour & get user media aren't compatible with e10s currenly.
  if [ "$1" != "--skip-e10s" ] && \
     [ "$test" != "browser/components/uitour/test/browser_UITour_loop.js" ] && \
     [ "$test" != "browser/base/content/test/general/browser_devices_get_user_media_about_urls.js" ];
  then
    ./mach mochitest $test
  fi
done
