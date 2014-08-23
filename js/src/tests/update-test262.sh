#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Abort when an error occurs.
set -e

# Updates the jstests copy of the test cases of Test262, the conformance test
# suite for ECMA-262 and ECMA-402, ECMAScript and its Internationalization API.

function usage()
{
  echo "Usage: update-test262.sh <URL of test262 hg> [clone | copy]"
  echo ""
  echo "The URL will most commonly be http://hg.ecmascript.org/tests/test262/ "
  echo "but may also be a local clone.  Don't use a local clone when generating"
  echo "the final import patch"\!"  test262/HG-INFO will record the wrong URL"
  echo "if you do so.  Local cloning is only useful when editing this script to"
  echo "import a larger test262 subset."
  echo ""
  echo "If a local clone is specified, the optional clone/copy argument will"
  echo "either clone into a temporary directory, or directly copy from the"
  echo "clone's checkout.  'clone' semantics are the default."
  exit 1
}

if [ $# -lt 1 ]; then
  usage
elif [ $# -eq 1 -o "$2" == "clone" ]; then
  # Beware!  'copy' support requires that the clone performed here *never* be
  # altered.  If it were altered, those changes wouldn't appear in the final
  # set of changes as determined by the 'copy' path below.

  # Mercurial doesn't have a way to download just a part of a repository, or to
  # just get the working copy - we have to clone the entire thing. We use a
  # temporary test262 directory for that.
  unique_dir=`mktemp -d /tmp/test262.XXXX` || exit 1
  tmp_dir=${unique_dir}/test262

  # Remove the temporary test262 directory on exit.
  function cleanupTempFiles()
  {
    rm -rf ${unique_dir}
  }
  trap cleanupTempFiles EXIT

  echo "Feel free to get some coffee - this could take a few minutes..."
  hg clone $1 ${tmp_dir}
elif [ "$2" == "copy" ]; then
  echo "Copying directly from $1; be sure this repository is updated to tip"\!
  tmp_dir="$1"
else
  usage
fi

# Now to the actual test262 directory.
js_src_tests_dir=`dirname $0`
test262_dir=${js_src_tests_dir}/test262
rm -rf ${test262_dir}
mkdir ${test262_dir}

# Copy over the test262 license.
cp ${tmp_dir}/LICENSE ${test262_dir}

# The test262 tests are in test/suite.  The "bestPractice" tests cover non-
# standard extensions, or are not strictly required by specs, so we don't
# include them.  The remaining tests are currently in "intl402" or "chNN"
# directories (where NN is an ECMA-262 chapter number).  This may change at
# some point, as there is some dissatisfaction on test262-discuss with using
# impermanent section numbering (across ES5, ES6, etc.) to identify what's
# tested.
#
# The large quantity of tests at issue here, and the variety of things being
# tested, motivate importing portions of test262 incrementally.  So rather than
# doing this:
#
#   cp -r ${tmp_dir}/test/suite/ch* ${test262_dir}
#
# ...we instead individually import folders whose tests we pass.  (Well, mostly
# pass -- see the comment at the end of this script.)
cp -r ${tmp_dir}/test/suite/ch06 ${test262_dir}/ch06
cp -r ${tmp_dir}/test/suite/ch07 ${test262_dir}/ch07
cp -r ${tmp_dir}/test/suite/ch08 ${test262_dir}/ch08
cp -r ${tmp_dir}/test/suite/ch09 ${test262_dir}/ch09
cp -r ${tmp_dir}/test/suite/ch10 ${test262_dir}/ch10
cp -r ${tmp_dir}/test/suite/ch11 ${test262_dir}/ch11
cp -r ${tmp_dir}/test/suite/ch12 ${test262_dir}/ch12
cp -r ${tmp_dir}/test/suite/ch13 ${test262_dir}/ch13
cp -r ${tmp_dir}/test/suite/ch14 ${test262_dir}/ch14

# The test402 tests are in test/suite/intl402/.  For now there are no
# "bestPractice" tests to omit.  The remaining tests are in chNN directories,
# NN referring to chapters of ECMA-402.
#
# All intl402 tests are runnable, and only a few currently fail, so we import
# them wildcard-style.
mkdir ${test262_dir}/intl402
cp -r ${tmp_dir}/test/suite/intl402/ch* ${test262_dir}/intl402

# Copy over harness supporting files needed by the test402 tests.
cp ${tmp_dir}/test/harness/sta.js ${js_src_tests_dir}/supporting/
cp ${tmp_dir}/test/harness/testBuiltInObject.js ${js_src_tests_dir}/supporting/
cp ${tmp_dir}/test/harness/testIntl.js ${js_src_tests_dir}/supporting/

# Create empty browser.js and shell.js in all test directories to keep
# jstests happy.
for dir in `find ${test262_dir} ${test262_dir}/ch* ${test262_dir}/intl402/ch* -type d -print` ; do
  touch $dir/browser.js
  touch $dir/shell.js
done

# Construct the test262 tests' jstests adapter files.
cp ${js_src_tests_dir}/supporting/test262-browser.js ${test262_dir}/browser.js
cat ${js_src_tests_dir}/supporting/sta.js ${js_src_tests_dir}/supporting/test262-shell.js > ${test262_dir}/shell.js

# Restore the Intl tests' jstests adapter files. Loading include files into the
# browser from a script so that they become synchronously available to that same
# script is basically impossible. Instead, just concatenate all the scripts
# together into one script loaded before the test executes.
cat ${js_src_tests_dir}/supporting/test402-browser.js \
    ${js_src_tests_dir}/supporting/testBuiltInObject.js \
    ${js_src_tests_dir}/supporting/testIntl.js > ${test262_dir}/intl402/browser.js
cp ${js_src_tests_dir}/supporting/test402-shell.js ${test262_dir}/intl402/shell.js

# Keep a record of what we imported.
echo "URL:         $1" > ${test262_dir}/HG-INFO
hg -R ${tmp_dir} log -r. >> ${test262_dir}/HG-INFO

# Update for the patch.
hg addremove ${js_src_tests_dir}/supporting
hg addremove ${test262_dir}

# Apply a very narrow set of post-fixes to tests that haven't yet been updated
# for spec changes, or where we're experimenting with semantics that disagree
# with those in test262.  See below: this isn't every test we don't pass!
patch -d "${js_src_tests_dir}" -p4 < ./function-arguments-caller-changes.diff
patch -d "${js_src_tests_dir}" -p4 < ./dupl-prop-changes.diff

# The alert reader may now be wondering: what about test262 tests that we don't
# pass?  (And what about any test262 tests whose format we don't yet support?)
# We explicitly disable all such tests in js/src/tests/jstests.list one by one.
# This has the moderate benefit that if a bug is fixed, only that one file must
# be updated, and we don't have to rerun this script.
