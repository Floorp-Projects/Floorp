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

import xpcom
from xpcom import components

import factory
import module

import glob, os, types
import traceback

from xpcom.client import Component

# Until we get interface constants.
When_Startup = 0
When_Component = 1
When_Timer = 2

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
    pcl = PythonComponentLoader
    from xpcom import _xpcom
    svc = _xpcom.GetGlobalServiceManager().getServiceByContractID("@mozilla.org/categorymanager;1", components.interfaces.nsICategoryManager)
    svc.addCategoryEntry("component-loader", pcl._reg_component_type_, pcl._reg_contractid_, 1, 1)

class PythonComponentLoader:
    _com_interfaces_ = components.interfaces.nsIComponentLoader
    _reg_clsid_ = "{63B68B1E-3E62-45f0-98E3-5E0B5797970C}" # Never copy these!
    _reg_contractid_ = "moz.pyloader.1"
    _reg_desc_ = "Python component loader"
    # Optional function which performs additional special registration
    # Appears that no special unregistration is needed for ComponentLoaders, hence no unregister function.
    _reg_registrar_ = (register_self,None)
    # Custom attributes for ComponentLoader registration.
    _reg_component_type_ = "script/python"

    def __init__(self):
        self.com_modules = {} # Keyed by module's FQN as obtained from nsIFile.path

    def _getCOMModuleForLocation(self, componentFile):
        fqn = componentFile.path
        mod = self.com_modules.get(fqn)
        if mod is not None:
            return mod
        import ihooks, sys
        base_name = os.path.splitext(os.path.basename(fqn))[0]
        loader = ihooks.ModuleLoader()

        module_name_in_sys = "component:%s" % (base_name,)
        stuff = loader.find_module(base_name, [componentFile.parent.path])
        assert stuff is not None, "Couldnt find the module '%s'" % (base_name,)
        py_mod = loader.load_module( module_name_in_sys, stuff )

        # Make and remember the COM module.
        comps = FindCOMComponents(py_mod)
        mod = module.Module(comps)
        
        self.com_modules[fqn] = mod
        return mod
        
    def getFactory(self, clsid, location, type):
        # return the factory
        assert type == self._reg_component_type_, "Being asked to create an object not of my type:%s" % (type,)
        file_interface = components.manager.specForRegistryLocation(location)
        # delegate to the module.
        m = self._getCOMModuleForLocation(file_interface)
        return m.getClassObject(components.manager, clsid, components.interfaces.nsIFactory)

    def init(self, comp_mgr, registry):
        # void
        self.comp_mgr = comp_mgr
        if xpcom.verbose:
            print "Python component loader init() called"

     # Called when a component of the appropriate type is registered,
     # to give the component loader an opportunity to do things like
     # annotate the registry and such.
    def onRegister (self, clsid, type, className, proId, location, replace, persist):
        if xpcom.verbose:
            print "Python component loader - onRegister() called"

    def autoRegisterComponents (self, when, directory):
        directory_path = directory.path
        if xpcom.verbose:
            print "Auto-registering all Python components in", directory_path

        # ToDo - work out the right thing here
        # eg - do we recurse?
        # - do we support packages?
        entries = directory.directoryEntries
        while entries.HasMoreElements():
            entry = entries.GetNext(components.interfaces.nsIFile)
            if os.path.splitext(entry.path)[1]==".py":
                try:
                    self.autoRegisterComponent(when, entry)
                # Handle some common user errors
                except xpcom.COMException, details:
                    from xpcom import nsError
                    # If the interface name does not exist, suppress the traceback
                    if details.errno==nsError.NS_ERROR_NO_INTERFACE:
                        print "** Registration of '%s' failed\n %s" % (entry.leafName,details.message,)
                    else:
                        print "** Registration of '%s' failed!" % (entry.leafName,)
                        traceback.print_exc()
                except SyntaxError, details:
                    # Syntax error in source file - no useful traceback here either.
                    print "** Registration of '%s' failed!" % (entry.leafName,)
                    traceback.print_exception(SyntaxError, details, None)
                except:
                    # All other exceptions get the full traceback.
                    print "** Registration of '%s' failed!" % (entry.leafName,)
                    traceback.print_exc()

    def autoRegisterComponent (self, when, componentFile):
        # bool return
        
        # Check if we actually need to do anything
        modtime = componentFile.lastModifiedTime
        loader_mgr = components.manager.queryInterface(components.interfaces.nsIComponentLoaderManager)
        if not loader_mgr.hasFileChanged(componentFile, None, modtime):
            return 1

        # auto-register via the module.
        m = self._getCOMModuleForLocation(componentFile)
        m.registerSelf(components.manager, componentFile, None, self._reg_component_type_)
        loader_mgr = components.manager.queryInterface(components.interfaces.nsIComponentLoaderManager)
        loader_mgr.saveFileInfo(componentFile, None, modtime)
        return 1

    def autoUnregisterComponent (self, when, componentFile):
        # bool return
        # auto-unregister via the module.
        m = self._getCOMModuleForLocation(componentFile)
        loader_mgr = components.manager.queryInterface(components.interfaces.nsIComponentLoaderManager)
        try:
            m.unregisterSelf(components.manager, componentFile)
        finally:
            loader_mgr.removeFileInfo(componentFile, None)
        return 1

    def registerDeferredComponents (self, when):
        # bool return
        if xpcom.verbose:
            print "Python component loader - registerDeferred() called"
        return 0 # no more to register
    def unloadAll (self, when):
        if xpcom.verbose:
            print "Python component loader being asked to unload all components!"
        self.comp_mgr = None
        self.com_modules = {}

def MakePythonComponentLoaderModule(serviceManager, nsIFile):
    import module
    return module.Module( [PythonComponentLoader] )
