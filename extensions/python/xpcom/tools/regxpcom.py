# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with the
# License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
# the specific language governing rights and limitations under the License.
#
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is ActiveState Tool Corp.
# Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
# ActiveState Tool Corp.  All Rights Reserved.
#
# Contributor(s): Mark Hammond <MarkH@ActiveState.com> (original author)
#

# regxpcom.py - basically the standard regxpcom.cpp ported to Python.

from xpcom import components, _xpcom
import sys
import os

def ProcessArgs(args):
    unregister = 0
    for arg in args:
        if arg == "-u":
            unregister = 1
        else:
            spec = components.classes['@mozilla.org/file/local;1'].createInstance()
            spec = spec.QueryInterface(components.interfaces.nsILocalFile)
            spec.initWithPath(os.path.abspath(arg))
            if unregister:
                components.manager.autoUnregisterComponent( components.manager.NS_Startup, spec)
                print "Successfully unregistered", spec.path
            else:
                components.manager.autoRegisterComponent( components.manager.NS_Startup, spec)
                print "Successfully registered", spec.path
            unregister = 0

import xpcom
if len(sys.argv) < 2:
    components.manager.autoRegister( components.manager.NS_Startup, None)
else:
    ProcessArgs(sys.argv[1:])

_xpcom.NS_ShutdownXPCOM()