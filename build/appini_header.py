# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is a build helper for libraries
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
# Mike Hommey <mh@glandium.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

'''Parses a given application.ini file and outputs the corresponding
   XULAppData structure as a C++ header file'''

import ConfigParser
import sys

def main(file):
    config = ConfigParser.RawConfigParser()
    config.read(file)
    flags = set()
    try:
        if config.getint('XRE', 'EnableExtensionManager') == 1:
            flags.add('NS_XRE_ENABLE_EXTENSION_MANAGER')
    except: pass
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

    print '''#include "nsXULAppAPI.h"
             static const nsXREAppData sAppData = {
                 sizeof(nsXREAppData),
                 NULL, // directory
                 "%(App:vendor)s",
                 "%(App:name)s",
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
             };''' % appdata

if __name__ == '__main__':
    if len(sys.argv) != 1:
        main(sys.argv[1])
    else:
        print >>sys.stderr, "Usage: %s /path/to/application.ini" % sys.argv[0]
