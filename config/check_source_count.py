#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Usage: check_source_count.py SEARCH_TERM COUNT ERROR_LOCATION REPLACEMENT [FILES...]
#   Checks that FILES contains exactly COUNT matches of SEARCH_TERM. If it does
#   not, an error message is printed, quoting ERROR_LOCATION, which should
#   probably be the filename and line number of the erroneous call to
#   check_source_count.py.
from __future__ import print_function
import sys
import re

search_string = sys.argv[1]
expected_count = int(sys.argv[2])
error_location = sys.argv[3]
replacement = sys.argv[4]
files = sys.argv[5:]

details = {}

count = 0
for f in files:
    text = file(f).read()
    match = re.findall(search_string, text)
    if match:
        num = len(match)
        count += num
        details[f] = num

if count == expected_count:
    print("TEST-PASS | check_source_count.py {0} | {1}"
          .format(search_string, expected_count))

else:
    print("TEST-UNEXPECTED-FAIL | check_source_count.py {0} | "
          .format(search_string),
          end='')
    if count < expected_count:
        print("There are fewer occurrences of /{0}/ than expected. "
              "This may mean that you have removed some, but forgotten to "
              "account for it {1}.".format(search_string, error_location))
    else:
        print("There are more occurrences of /{0}/ than expected. We're trying "
              "to prevent an increase in the number of {1}'s, using {2} if "
              "possible. If it is unavoidable, you should update the expected "
              "count {3}.".format(search_string, search_string, replacement,
                                  error_location))

    print("Expected: {0}; found: {1}".format(expected_count, count))
    for k in sorted(details):
        print("Found {0} occurences in {1}".format(details[k], k))
    sys.exit(-1)
