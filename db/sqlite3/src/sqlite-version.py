# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import re

# Define RegEx for finding and breaking apart SQLITE_VERSION string
versionString = "^#define SQLITE_VERSION\D*(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?\D*"

#Use command line argument pointing to sqlite3.h to open the file and use
#the RegEx to search for the line #define SQLITE_VERSION. When the RegEx
#matches and the line is found, print the version strings to the console
#with #definey goodness.
for line in open(sys.argv[1]):
    result = re.search(versionString, line)
    if result is not None:    #If RegEx matches, print version numbers and stop
        splitVersion = list(result.groups())
        if splitVersion[3] is None:       #Make 4th list element 0 if undefined
            splitVersion[3:] = ['0']
        print "#define SQLITE_VERSION_MAJOR " + splitVersion[0]
        print "#define SQLITE_VERSION_MINOR " + splitVersion[1]
        print "#define SQLITE_VERSION_PATCH " + splitVersion[2]
        print "#define SQLITE_VERSION_SUBPATCH " + splitVersion[3]
        break
