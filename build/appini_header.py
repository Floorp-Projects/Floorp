# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''Parses a given application.ini file and outputs the corresponding
   XULAppData structure as a C++ header file'''

import ConfigParser
import sys

def main(output, file):
    config = ConfigParser.RawConfigParser()
    config.read(file)
    flags = set()
    try:
        if config.getint('XRE', 'EnableProfileMigrator') == 1:
            flags.add('NS_XRE_ENABLE_PROFILE_MIGRATOR')
    except: pass
    try:
        if config.getint('Crash Reporter', 'Enabled') == 1:
            flags.add('NS_XRE_ENABLE_CRASH_REPORTER')
    except: pass
    appdata = dict(("%s:%s" % (s, o), config.get(s, o)) for s in config.sections() for o in config.options(s))
    appdata['flags'] = ' | '.join(flags) if flags else '0'
    appdata['App:profile'] = '"%s"' % appdata['App:profile'] if 'App:profile' in appdata else 'NULL'
    expected = ('App:vendor', 'App:name', 'App:remotingname', 'App:version', 'App:buildid',
                'App:id', 'Gecko:minversion', 'Gecko:maxversion')
    missing = [var for var in expected if var not in appdata]
    if missing:
        print >>sys.stderr, \
            "Missing values in %s: %s" % (file, ', '.join(missing))
        sys.exit(1)

    if not 'Crash Reporter:serverurl' in appdata:
        appdata['Crash Reporter:serverurl'] = ''

    output.write('''#include "nsXREAppData.h"
             static const nsXREAppData sAppData = {
                 sizeof(nsXREAppData),
                 NULL, // directory
                 "%(App:vendor)s",
                 "%(App:name)s",
                 "%(App:remotingname)s",
                 "%(App:version)s",
                 "%(App:buildid)s",
                 "%(App:id)s",
                 NULL, // copyright
                 %(flags)s,
                 NULL, // xreDirectory
                 "%(Gecko:minversion)s",
                 "%(Gecko:maxversion)s",
                 "%(Crash Reporter:serverurl)s",
                 %(App:profile)s
             };''' % appdata)

if __name__ == '__main__':
    if len(sys.argv) != 1:
        main(sys.stdout, sys.argv[1])
    else:
        print >>sys.stderr, "Usage: %s /path/to/application.ini" % sys.argv[0]
