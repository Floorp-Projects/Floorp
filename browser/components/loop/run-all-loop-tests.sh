#!/bin/sh
# Run from topsrcdir, no args

set -e

# Main tests
./mach xpcshell-test browser/components/loop/
./mach marionette-test browser/components/loop/manifest.ini

# The browser_parsable_css.js can fail if we add some css that isn't parsable.
#
# The check to make sure that the media devices can be used in Loop without
# prompting is in browser_devices_get_user_media_about_urls.js. It's possible
# to mess this up with CSP handling, and probably other changes, too.

./mach mochitest \
  browser/components/loop/test/mochitest \
  browser/modules/test/browser_UITour_loop.js \
  browser/base/content/test/general/browser_devices_get_user_media_about_urls.js

# This is currently disabled because the test itself is busted.  Once bug
# 1062821 is landed, we should see if things work again, and then re-enable it.
# The re-enabling is tracked in bug 1113350.
#
#  browser/base/content/test/general/browser_parsable_css.js \
