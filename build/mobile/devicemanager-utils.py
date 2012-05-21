# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
