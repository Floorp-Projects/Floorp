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
        self.klassFactory = factory.Factory

    def getClassObject(self, compMgr, clsid, iid):
        # Single retval result.
        try:
            klass = self.components[clsid]
        except KeyError:
            raise ServerException(nsError.NS_ERROR_FACTORY_NOT_REGISTERED)
        
        # We can ignore the IID - the auto-wrap process will automatically QI us.
        return self.klassFactory(klass)

    def registerSelf(self, compMgr, location, loaderStr, componentType):
        # void function.
        fname = os.path.basename(location.path)
        for klass in self.components.values():
            reg_contractid = klass._reg_contractid_
            print "Registering '%s' (%s)" % (reg_contractid, fname)
            reg_desc = getattr(klass, "_reg_desc_", reg_contractid)
            compMgr = compMgr.queryInterface(components.interfaces.nsIComponentRegistrar)
            compMgr.registerFactoryLocation(klass._reg_clsid_,
                                              reg_desc,
                                              reg_contractid,
                                              location,
                                              loaderStr,
                                              componentType)

            # See if this class nominates custom register_self
            extra_func = getattr(klass, "_reg_registrar_", (None,None))[0]
            if extra_func is not None:
                extra_func(klass, compMgr, location, loaderStr, componentType)

    def unregisterSelf(self, compMgr, location, loaderStr):
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
                    extra_func(klass, compMgr, location, loaderStr)
                except Exception:
                    ok = 0
            if ok:
                print "Successfully unregistered", klass.__name__
            else:
                print "Unregistration of", klass.__name__, "failed. (probably just not already registered)"
        
    def canUnload(self, compMgr):
        # single bool result
        return 0 # we can never unload!

