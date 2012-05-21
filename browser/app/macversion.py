#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from optparse import OptionParser
import sys
import re

o = OptionParser()
o.add_option("--buildid", dest="buildid")
o.add_option("--version", dest="version")

(options, args) = o.parse_args()

if not options.buildid:
    print >>sys.stderr, "--buildid is required"
    sys.exit(1)

if not options.version:
    print >>sys.stderr, "--version is required"
    sys.exit(1)

# We want to build a version number that matches the format allowed for
# CFBundleVersion (nnnnn[.nn[.nn]]). We'll incorporate both the version
# number as well as the date, so that it changes at least daily (for nightly
# builds), but also so that newly-built older versions (e.g. beta build) aren't
# considered "newer" than previously-built newer versions (e.g. a trunk nightly)

buildid = open(options.buildid, 'r').read()

# extract only the major version (i.e. "14" from "14.0b1")
majorVersion = re.match(r'^(\d+)[^\d].*', options.version).group(1)
# last two digits of the year
twodigityear = buildid[2:4]
month = buildid[4:6]
if month[0] == '0':
  month = month[1]
day = buildid[6:8]
if day[0] == '0':
  day = day[1]

print '%s.%s.%s' % (majorVersion + twodigityear, month, day)
