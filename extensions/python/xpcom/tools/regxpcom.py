# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

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