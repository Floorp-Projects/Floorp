#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Updates the jstests copy of the test cases of Test402, the conformance test
# suite for standard ECMA-402, ECMAScript Internationalization API
# Specification.

# Usage: update-test402.sh <URL of test262 hg>
# E.g.: update-test402.sh http://hg.ecmascript.org/tests/test262/
# Note that test402 is part of the test262 repository.

# Abort when an error occurs.
set -e

if [ $# -lt 1 ]; then
  echo "Usage: update-test402.sh <URL of test262 hg>"
  exit 1
fi

# Test402 in its original form uses the harness of Test262, the conformance
# test suite for standard ECMA-262, ECMAScript Language Specification, and its
# test cases are part of the test262 repository.
# Mercurial doesn't have a way to download just a part of a repository, or to
# just get the working copy - we have to clone the entire thing. We use a
# temporary test262 directory for that.
tmp_dir=`mktemp -d /tmp/test402.XXXX`/test262 || exit 1
echo "Feel free to get some coffee - this could take a few minutes..."
hg clone $1 ${tmp_dir}

# Now to the actual test402 directory.
test402_dir=`dirname $0`/test402
rm -rf ${test402_dir}
mkdir ${test402_dir}
mkdir ${test402_dir}/lib

# Copy over the test402 tests, the supporting JavaScript library files, and
# the license.
cp -r ${tmp_dir}/test/suite/intl402/ch* ${test402_dir}
cp ${tmp_dir}/test/harness/testBuiltInObject.js ${test402_dir}/lib
cp ${tmp_dir}/test/harness/testIntl.js ${test402_dir}/lib
cp ${tmp_dir}/LICENSE ${test402_dir}

# Create empty browser.js and shell.js in each test directory to keep
# jstests happy.
for dir in `find test402/ch* -type d -print` ; do
    touch $dir/browser.js
    touch $dir/shell.js
done

# Restore our own jstests adapter files.
cp supporting/test402-browser.js test402/browser.js
cp supporting/test402-shell.js test402/shell.js

# Keep a record of what we imported.
echo "URL:         $1" > ${test402_dir}/HG-INFO
hg -R ${tmp_dir} log -r. >> ${test402_dir}/HG-INFO

# Update for the patch.
hg addremove ${test402_dir}

# Get rid of the Test262 clone.
rm -rf ${tmp_dir}

# The alert reader may now be wondering: what about test402 tests we don't
# currently pass?  We disable such tests in js/src/tests/jstests.list, one by
# one.  This has the moderate benefit that if a bug is fixed, only that one file
# must be updated, and we don't have to rerun this script.
