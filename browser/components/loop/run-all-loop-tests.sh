#!/bin/sh
# Run from topsrcdir, no args

set -e

# Main tests
./mach xpcshell-test browser/components/loop/
./mach marionette-test browser/components/loop/manifest.ini

# The browser_parsable_css.js can fail if we add some css that isn't parsable.
./mach mochitest browser/components/loop/test/mochitest browser/base/content/test/general/browser_parsable_css.js
