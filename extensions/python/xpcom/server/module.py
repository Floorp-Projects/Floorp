# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

from xpcom import components
from xpcom import ServerException, Exception
from xpcom import nsError

import factory

import types
import os

class Module:
    _com_interfaces_ = components.interfaces.nsIModule
    def __init__(self, comps):
        # Build a map of classes we can provide factories for.
        c = self.components = {}
        for klass in comps:
            c[components.ID(klass._reg_clsid_)] = klass

    def getClassObject(self, compMgr, clsid, iid):
        # Single retval result.
        try:
            klass = self.components[clsid]
        except KeyError:
            raise ServerException(nsError.NS_ERROR_FACTORY_NOT_REGISTERED)
        
        # We can ignore the IID - the auto-wrapp process will automatically QI us.
        return factory.Factory(klass)

    def registerSelf(self, compMgr, location, registryLocation, componentType):
        # void function.
        for klass in self.components.values():
            print "Registering: %s" % (klass.__name__,)
            reg_contractid = klass._reg_contractid_
            reg_desc = getattr(klass, "_reg_desc_", reg_contractid)
            compMgr.registerComponentWithType(klass._reg_clsid_,
                                              reg_desc,
                                              reg_contractid,
                                              location,
                                              registryLocation,
                                              1,
                                              1,
                                              componentType)

            # See if this class nominates custom register_self
            extra_func = getattr(klass, "_reg_registrar_", (None,None))[0]
            if extra_func is not None:
                extra_func(compMgr, location, registryLocation, componentType)
        print "Registered %d Python components in %s" % (len(self.components),os.path.basename(location.path))

    def unregisterSelf(self, compMgr, location, registryLocation):
        # void function.
        for klass in self.components.values():
            ok = 1
            try:
                compMgr.unregisterComponentSpec(klass._reg_clsid_, location)
            except Exception:
                ok = 0
            # Give the class a bash even if we failed!
            extra_func = getattr(klass, "_reg_registrar_", (None,None))[1]
            if extra_func is not None:
                try:
                    extra_func(compMgr, location, registryLocation)
                except Exception:
                    ok = 0
            if ok:
                print "Successfully unregistered", klass.__name__
            else:
                print "Unregistration of", klass.__name__, "failed. (probably just not already registered)"
        
    def canUnload(self, compMgr):
        # single bool result
        return 0 # we can never unload!

