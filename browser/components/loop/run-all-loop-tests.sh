#!/bin/sh
# Run from topsrcdir, no args

set -e

# Main tests
./mach xpcshell-test browser/components/loop/
./mach marionette-test browser/components/loop/manifest.ini

# The browser_parsable_css.js can fail if we add some css that isn't parsable.
./mach mochitest browser/components/loop/test/mochitest browser/base/content/test/general/browser_parsable_css.js

# The check to make sure that the media devices can be used in Loop without
# prompting is in browser_devices_get_user_media_about_urls.js. It's possible
# to mess this up with CSP handling, and probably other changes, too.
./mach mochitest browser/base/content/test/general/browser_devices_get_user_media_about_urls.js
