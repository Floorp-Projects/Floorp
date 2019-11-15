#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This script modifies V8 regexp source files to make them suitable for
# inclusion in SpiderMonkey. Specifically, it:
#
# 1. Rewrites all #includes of V8 regexp headers to point to their location in
#    the SM tree: src/regexp/* --> new-regexp/*
# 2. Removes all #includes of other V8 src/* headers. The required definitions
#    will be provided by regexp-shim.h.
#
# Usage:
#    cd js/src/new-regexp
#    find . -name "*.h" -o -name "*.cc" | xargs ./update_headers.py
#

import fileinput
import re
import sys

# 1. Rewrite includes of V8 regexp headers
regexp_include = re.compile('#include "src/regexp')
regexp_include_new = '#include "new-regexp'

# 2. Remove includes of other V8 headers
other_include = re.compile('#include "src/')

for line in fileinput.input(inplace=1):
    if regexp_include.search(line):
        sys.stdout.write(re.sub(regexp_include, regexp_include_new, line))
    elif other_include.search(line):
        pass
    else:
        sys.stdout.write(line)
