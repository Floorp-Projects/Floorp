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
# The Original Code is remote test framework.
#
# The Initial Developer of the Original Code is
# the Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

import devicemanager
import sys
import os

def copy(dm, gre_path):
    file = sys.argv[2]
    if len(sys.argv) >= 4:
        path = sys.argv[3]
    else:
        path = gre_path
    if os.path.isdir(file):
        dm.pushDir(file, path.replace('\\','/'))
    else:
        dm.pushFile(file, path.replace('\\','/'))

def delete(dm, gre_path):
    file = sys.argv[2]
    dm.removeFile(file)

def main():
    ip_addr = os.environ.get("DEVICE_IP")
    port = os.environ.get("DEVICE_PORT")
    gre_path = os.environ.get("REMOTE_GRE_PATH").replace('\\','/')

    if ip_addr == None:
        print "Error: please define the environment variable DEVICE_IP before running this test"
        sys.exit(1)

    if port == None:
        print "Error: please define the environment variable DEVICE_PORT before running this test"
        sys.exit(1)

    if gre_path == None:
        print "Error: please define the environment variable REMOTE_GRE_PATH before running this test"
        sys.exit(1)

    dm = devicemanager.DeviceManager(ip_addr, int(port))
    dm.sendCMD(['cd '+ gre_path], sleep = 1)
    dm.debug = 0

    if len(sys.argv) < 3:
        print "usage: python devicemanager-utils.py <cmd> <path> [arg]"
    cmd = sys.argv[1]
    if (cmd == 'copy'):
        sys.exit(copy(dm, gre_path))
    if (cmd == 'delete'):
        sys.exit(delete(dm, gre_path))
    print "unrecognized command. supported commands are copy and delete"
    sys.exit(-1)

if __name__ == '__main__':
    main()
