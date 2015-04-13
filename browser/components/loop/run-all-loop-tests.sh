#!/bin/sh
# Run from topsrcdir, no args

if [ "$1" == "--help" ]; then
  echo "Usage: ./run-all-loop-tests.sh [options]"
  echo "    --skip-e10s  Skips the e10s tests"
  exit 0;
fi

set -e

# Main tests

LOOPDIR=browser/components/loop
ESLINT=standalone/node_modules/.bin/eslint
if [ -x "${LOOPDIR}/${ESLINT}" ]; then
  echo 'running eslint; see http://eslint.org/docs/rules/ for error info'
  (cd ${LOOPDIR} && ./${ESLINT} --ext .js --ext .jsm --ext .jsx .)
  if [ $? != 0 ]; then
    exit 1;
  fi
  echo 'eslint run finished.'
fi

./mach xpcshell-test ${LOOPDIR}/
./mach marionette-test ${LOOPDIR}/manifest.ini

# The browser_parsable_css.js can fail if we add some css that isn't parsable.
#
# The check to make sure that the media devices can be used in Loop without
# prompting is in browser_devices_get_user_media_about_urls.js. It's possible
# to mess this up with CSP handling, and probably other changes, too.

TESTS="
  ${LOOPDIR}/test/mochitest
  browser/components/uitour/test/browser_UITour_loop.js
  browser/base/content/test/general/browser_devices_get_user_media_about_urls.js
"

./mach mochitest $TESTS

if [ "$1" != "--skip-e10s" ]; then
  ./mach mochitest --e10s $TESTS
fi

# This is currently disabled because the test itself is busted.  Once bug
# 1062821 is landed, we should see if things work again, and then re-enable it.
# The re-enabling is tracked in bug 1113350.
#
#  browser/base/content/test/general/browser_parsable_css.js \
