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
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is
# Activestate Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#    Mark Hammond <MarkH@ActiveState.com>
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

import xpcom
from xpcom import components, nsError
import xpcom.shutdown

import module

import os, types

def _has_good_attr(object, attr):
    # Actually allows "None" to be specified to disable inherited attributes.
    return getattr(object, attr, None) is not None

def FindCOMComponents(py_module):
    # For now, just run over all classes looking for likely candidates.
    comps = []
    for name, object in py_module.__dict__.items():
        if type(object)==types.ClassType and \
           _has_good_attr(object, "_com_interfaces_") and \
           _has_good_attr(object, "_reg_clsid_") and \
           _has_good_attr(object, "_reg_contractid_"):
            comps.append(object)
    return comps

def register_self(klass, compMgr, location, registryLocation, componentType):
    pcl = ModuleLoader
    from xpcom import _xpcom
    svc = _xpcom.GetServiceManager().getServiceByContractID("@mozilla.org/categorymanager;1", components.interfaces.nsICategoryManager)
    # The category 'module-loader' is special - the component manager uses it
    # to create the nsIModuleLoader for a given component type.
    svc.addCategoryEntry("module-loader", pcl._reg_component_type_, pcl._reg_contractid_, 1, 1)

# The Python module loader.  Called by the component manager when it finds
# a component of type self._reg_component_type_.  Responsible for returning
# an nsIModule for the file.
class ModuleLoader:
    _com_interfaces_ = components.interfaces.nsIModuleLoader
    _reg_clsid_ = "{945BFDA9-0226-485e-8AE3-9A2F68F6116A}" # Never copy these!
    _reg_contractid_ = "@mozilla.org/module-loader/python;1"
    _reg_desc_ = "Python module loader"
    # Optional function which performs additional special registration
    # Appears that no special unregistration is needed for ModuleLoaders, hence no unregister function.
    _reg_registrar_ = (register_self,None)
    # Custom attributes for ModuleLoader registration.
    _reg_component_type_ = "application/x-python"

    def __init__(self):
        self.com_modules = {} # Keyed by module's FQN as obtained from nsIFile.path
        self.moduleFactory = module.Module
        xpcom.shutdown.register(self._on_shutdown)

    def _on_shutdown(self):
        self.com_modules.clear()

    def loadModule(self, aFile):
        return self._getCOMModuleForLocation(aFile)

    def _getCOMModuleForLocation(self, componentFile):
        fqn = componentFile.path
        if not fqn.endswith(".py"):
            raise xpcom.ServerException(nsError.NS_ERROR_INVALID_ARG)
        mod = self.com_modules.get(fqn)
        if mod is not None:
            return mod
        import ihooks, sys
        base_name = os.path.splitext(os.path.basename(fqn))[0]
        loader = ihooks.ModuleLoader()

        module_name_in_sys = "component:%s" % (base_name,)
        stuff = loader.find_module(base_name, [componentFile.parent.path])
        assert stuff is not None, "Couldn't find the module '%s'" % (base_name,)
        py_mod = loader.load_module( module_name_in_sys, stuff )

        # Make and remember the COM module.
        comps = FindCOMComponents(py_mod)
        mod = self.moduleFactory(comps)
        
        self.com_modules[fqn] = mod
        return mod
