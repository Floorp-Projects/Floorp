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
# ActiveState Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000, 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond <MarkH@ActiveState.com> (original author)
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

# This module provides the JavaScript "components" interface
import xpt
import xpcom, _xpcom
import xpcom.client
import xpcom.server
import types

StringTypes = [types.StringType, types.UnicodeType]

def _get_good_iid(iid):
    if iid is None:
        iid = _xpcom.IID_nsISupports
    elif type(iid) in StringTypes and len(iid)>0 and iid[0] != "{":
        iid = getattr(interfaces, iid)
    return iid

# The "manager" object.
manager = xpcom.client.Component(_xpcom.NS_GetGlobalComponentManager(), _xpcom.IID_nsIComponentManager)

# The "interfaceInfoManager" object - JS doesnt have this.
interfaceInfoManager = _xpcom.XPTI_GetInterfaceInfoManager()

# The serviceManager - JS doesnt have this either!
serviceManager = _xpcom.GetGlobalServiceManager()

# The "Exception" object
Exception = xpcom.COMException

# Base class for our collections.
# It appears that all objects supports "." and "[]" notation.
# eg, "interface.nsISupports" or interfaces["nsISupports"]
class _ComponentCollection:
    # Bases are to over-ride 2 methods.
    # _get_one(self, name) - to return one object by name
    # _build_dict - to return a dictionary which provide access into
    def __init__(self):
        self._dict_data = None
    def keys(self):
        if self._dict_data is None:
            self._dict_data = self._build_dict()
        return self._dict_data.keys()
    def items(self):
        if self._dict_data is None:
            self._dict_data = self._build_dict()
        return self._dict_data.items()
    def values(self):
        if self._dict_data is None:
            self._dict_data = self._build_dict()
        return self._dict_data.values()
    def has_key(self, key):
        if self._dict_data is None:
            self._dict_data = self._build_dict()
        return self._dict_data.has_key(key)

    def __len__(self):
        if self._dict_data is None:
            self._dict_data = self._build_dict()
        return len(self._dict_data)

    def __getattr__(self, attr):
        if self._dict_data is not None and self._dict_data.has_key(attr):
            return self._dict_data[attr]
        return self._get_one(attr)
    def __getitem__(self, item):
        if self._dict_data is not None and self._dict_data.has_key(item):
            return self._dict_data[item]
        return self._get_one(item)

_constants_by_iid_map = {}

class _Interface:
    # An interface object.
    def __init__(self, name, iid):
        # Bypass self.__setattr__ when initializing attributes.
        d = self.__dict__
        d['_iidobj_'] = iid # Allows the C++ framework to treat this as a native IID.
        d['name'] = name
    def __cmp__(self, other):
        this_iid = self._iidobj_
        other_iid = getattr(other, "_iidobj_", other)
        return cmp(this_iid, other_iid)
    def __hash__(self):
        return hash(self._iidobj_)
    def __str__(self):
        return str(self._iidobj_)
    def __getitem__(self, item):
        raise TypeError, "components.interface objects are not subscriptable"
    def __setitem__(self, item, value):
        raise TypeError, "components.interface objects are not subscriptable"
    def __setattr__(self, attr, value):
        raise AttributeError, "Can not set attributes on components.Interface objects"
    def __getattr__(self, attr):
        # Support constants as attributes.
        c = _constants_by_iid_map.get(self._iidobj_)
        if c is None:
            c = {}
            i = xpt.Interface(self._iidobj_)
            for c_ob in i.constants:
                c[c_ob.name] = c_ob.value
            _constants_by_iid_map[self._iidobj_] = c
        if c.has_key(attr):
            return c[attr]
        raise AttributeError, "'%s' interfaces do not define a constant '%s'" % (self.name, attr)


class _Interfaces(_ComponentCollection):
    def _get_one(self, name):
        try:
            item = interfaceInfoManager.GetInfoForName(name)
        except xpcom.COMException, why:
            # Present a better exception message, and give a more useful error code.
            import nsError
            raise xpcom.COMException(nsError.NS_ERROR_NO_INTERFACE, "The interface '%s' does not exist" % (name,))
        return _Interface(item.GetName(), item.GetIID())

    def _build_dict(self):
        ret = {}
        enum = interfaceInfoManager.EnumerateInterfaces()
        while not enum.IsDone():
            # Call the Python-specific FetchBlock, to keep the loop in C.
            items = enum.FetchBlock(500, _xpcom.IID_nsIInterfaceInfo)
            # This shouldnt be necessary, but appears to be so!
            for item in items:
                ret[item.GetName()] = _Interface(item.GetName(), item.GetIID())
        return ret

# And the actual object people use.
interfaces = _Interfaces()

del _Interfaces # Keep our namespace clean.

#################################################
class _Class:
    def __init__(self, contractid):
        self.contractid = contractid
    def __getattr__(self, attr):
        if attr == "clsid":
            rc = manager.contractIDToClassID(self.contractid)
            # stash it away - it can never change!
            self.clsid = rc
            return rc
        raise AttributeError, "%s class has no attribute '%s'" % (self.contractid, attr)
    def createInstance(self, iid = None):
        import xpcom.client
        try:
            return xpcom.client.Component(self.contractid, _get_good_iid(iid))
        except xpcom.COMException, details:
            import nsError
            # Handle "no such component" in a cleaner way for the user.
            if details.errno == nsError.NS_ERROR_FACTORY_NOT_REGISTERED:
                raise xpcom.COMException(details.errno, "No such component '%s'" % (self.contractid,))
            raise # Any other exception reraise.
    def getService(self, iid = None):
        return serviceManager.getServiceByContractID(self.contractid, _get_good_iid(iid))

class _Classes(_ComponentCollection):
    def __init__(self):
        _ComponentCollection.__init__(self)
    def _get_one(self, name):
        # XXX - Need to check the contractid is valid!
        return _Class(name)

    def _build_dict(self):
        ret = {}
        enum = manager.EnumerateContractIDs()
        while not enum.IsDone():
            # Call the Python-specific FetchBlock, to keep the loop in C.
            items = enum.FetchBlock(500)
            for item in items:
                name = str(item)
                ret[name] = _Class(name)
        return ret

classes = _Classes()

del _Classes

del _ComponentCollection

# The ID function
ID = _xpcom.IID

# A helper to cleanup our namespace as xpcom shuts down.
class _ShutdownObserver:
    _com_interfaces_ = interfaces.nsIObserver
    def observe(self, service, topic, extra):
        global manager, classes, interfaces, interfaceInfoManager, _shutdownObserver, serviceManager, _constants_by_iid_map
        manager = classes = interfaces = interfaceInfoManager = _shutdownObserver = serviceManager = _constants_by_iid_map = None
        xpcom.client._shutdown()
        xpcom.server._shutdown()

svc = _xpcom.GetGlobalServiceManager().getServiceByContractID("@mozilla.org/observer-service;1", interfaces.nsIObserverService)
# Observers will be QI'd for a weak-reference, so we must keep the
# observer alive ourself, and must keep the COM object alive,
# _not_ just the Python instance!!!
_shutdownObserver = xpcom.server.WrapObject(_ShutdownObserver(), interfaces.nsIObserver)
# Say we want a weak ref due to an assertion failing.  If this is fixed, we can pass 0,
# and remove the lifetime hacks above!  See http://bugzilla.mozilla.org/show_bug.cgi?id=99163
svc.addObserver(_shutdownObserver, "xpcom-shutdown", 1)
del svc, _ShutdownObserver
