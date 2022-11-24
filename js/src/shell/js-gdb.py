# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

""" GDB Python customization auto-loader for JS shell binary """

# This script will be installed into $objdir/dist/bin. Add $objdir to gdb's
# source search path and load in the Gecko+JS init file.

import os
import re
from os.path import abspath, dirname

import gdb

devel_objdir = abspath(os.path.join(dirname(__file__), "..", ".."))
m = re.search(r"[\w ]+: (.*)", gdb.execute("show dir", False, True))
if m and devel_objdir not in m.group(1).split(":"):
    gdb.execute("set dir {}:{}".format(devel_objdir, m.group(1)))

gdb.execute("source -s build/.gdbinit.loader")
