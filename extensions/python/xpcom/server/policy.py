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
# The Original Code is Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is
# ActiveState Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Mark Hammond <mhammond@skippinet.com.au> (original author)
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

from xpcom import xpcom_consts, _xpcom, client, nsError, ServerException, COMException
import xpcom
import traceback
import xpcom.server
import operator
import types

IID_nsISupports = _xpcom.IID_nsISupports
IID_nsIVariant = _xpcom.IID_nsIVariant
XPT_MD_IS_GETTER = xpcom_consts.XPT_MD_IS_GETTER
XPT_MD_IS_SETTER = xpcom_consts.XPT_MD_IS_SETTER

VARIANT_INT_TYPES = xpcom_consts.VTYPE_INT8, xpcom_consts.VTYPE_INT16, xpcom_consts.VTYPE_INT32, \
                    xpcom_consts.VTYPE_UINT8, xpcom_consts.VTYPE_UINT16, xpcom_consts.VTYPE_INT32
VARIANT_LONG_TYPES = xpcom_consts.VTYPE_INT64, xpcom_consts.VTYPE_UINT64
VARIANT_FLOAT_TYPES = xpcom_consts.VTYPE_FLOAT, xpcom_consts.VTYPE_DOUBLE
VARIANT_STRING_TYPES = xpcom_consts.VTYPE_CHAR, xpcom_consts.VTYPE_CHAR_STR, xpcom_consts.VTYPE_STRING_SIZE_IS, \
                       xpcom_consts.VTYPE_CSTRING
VARIANT_UNICODE_TYPES = xpcom_consts.VTYPE_WCHAR, xpcom_consts.VTYPE_DOMSTRING, xpcom_consts.VTYPE_WSTRING_SIZE_IS, \
                        xpcom_consts.VTYPE_ASTRING 

_supports_primitives_map_ = {} # Filled on first use.

_interface_sequence_types_ = types.TupleType, types.ListType
_string_types_ = types.StringType, types.UnicodeType
XPTI_GetInterfaceInfoManager = _xpcom.XPTI_GetInterfaceInfoManager

def _GetNominatedInterfaces(obj):
    ret = getattr(obj, "_com_interfaces_", None)
    if ret is None: return None
    # See if the user only gave one.
    if type(ret) not in _interface_sequence_types_:
        ret = [ret]
    real_ret = []
    # For each interface, walk to the root of the interface tree.
    iim = XPTI_GetInterfaceInfoManager()
    for interface in ret:
        # Allow interface name or IID.
        interface_info = None
        if type(interface) in _string_types_:
            try:
                interface_info = iim.GetInfoForName(interface)
            except COMException:
                pass
        if interface_info is None:
            # Allow a real IID
            interface_info = iim.GetInfoForIID(interface)
        real_ret.append(interface_info.GetIID())
        parent = interface_info.GetParent()
        while parent is not None:
            parent_iid = parent.GetIID()
            if parent_iid == IID_nsISupports:
                break
            real_ret.append(parent_iid)
            parent = parent.GetParent()
    return real_ret

##
## ClassInfo support
##
## We cache class infos by class
class_info_cache = {}

def GetClassInfoForObject(ob):
    if xpcom.server.tracer_unwrap is not None:
        ob = xpcom.server.tracer_unwrap(ob)
    klass = ob.__class__
    ci = class_info_cache.get(klass)
    if ci is None:
        ci = DefaultClassInfo(klass)
        ci = xpcom.server.WrapObject(ci, _xpcom.IID_nsIClassInfo, bWrapClient = 0)
        class_info_cache[klass] = ci
    return ci

class DefaultClassInfo:
    _com_interfaces_ = _xpcom.IID_nsIClassInfo
    def __init__(self, klass):
        self.klass = klass
        self.contractID = getattr(klass, "_reg_contractid_", None)
        self.classDescription = getattr(klass, "_reg_desc_", None)
        self.classID = getattr(klass, "_reg_clsid_", None)
        self.implementationLanguage = 3 # Python - avoid lookups just for this
        self.flags = 0 # what to do here??
        self.interfaces = None

    def get_classID(self):
        if self.classID is None:
            raise ServerException(nsError.NS_ERROR_NOT_IMPLEMENTED, "Class '%r' has no class ID" % (self.klass,))
        return self.classID

    def getInterfaces(self):
        if self.interfaces is None:
            self.interfaces = _GetNominatedInterfaces(self.klass)
        return self.interfaces

    def getHelperForLanguage(self, language):
        return None # Not sure what to do here.

class DefaultPolicy:
    def __init__(self, instance, iid):
        self._obj_ = instance
        self._nominated_interfaces_ = ni = _GetNominatedInterfaces(instance)
        self._iid_ = iid
        if ni is None:
            raise ValueError, "The object '%r' can not be used as a COM object" % (instance,)
        # This is really only a check for the user
        if __debug__:
            if iid != IID_nsISupports and iid not in ni:
                # The object may delegate QI.
                delegate_qi = getattr(instance, "_query_interface_", None)
                # Perform the actual QI and throw away the result - the _real_
                # QI performed by the framework will set things right!
                if delegate_qi is None or not delegate_qi(iid):
                    raise ServerException(nsError.NS_ERROR_NO_INTERFACE)
        # Stuff for the magic interface conversion.
        self._interface_info_ = None
        self._interface_iid_map_ = {} # Cache - Indexed by (method_index, param_index)

    def _QueryInterface_(self, com_object, iid):
        # Framework allows us to return a single boolean integer,
        # or a COM object.
        if iid in self._nominated_interfaces_:
            # We return the underlying object re-wrapped
            # in a new gateway - which is desirable, as one gateway should only support
            # one interface (this wont affect the users of this policy - we can have as many
            # gateways as we like pointing to the same Python objects - the users never
            # see what object the call came in from.
            # NOTE: We could have simply returned the instance and let the framework
            # do the auto-wrap for us - but this way we prevent a round-trip back into Python
            # code just for the autowrap.
            return xpcom.server.WrapObject(self._obj_, iid, bWrapClient = 0)

        # Always support nsIClassInfo 
        if iid == _xpcom.IID_nsIClassInfo:
            return GetClassInfoForObject(self._obj_)

        # See if the instance has a QI
        # use lower-case "_query_interface_" as win32com does, and it doesnt really matter.
        delegate = getattr(self._obj_, "_query_interface_", None)
        if delegate is not None:
            # The COM object itself doesnt get passed to the child
            # (again, as win32com doesnt).  It is rarely needed
            # (in win32com, we dont even pass it to the policy, although we have identified
            # one place where we should - for marshalling - so I figured I may as well pass it
            # to the policy layer here, but no all the way down to the object.
            return delegate(iid)
        # Finally see if we are being queried for one of the "nsISupports primitives"
        if not _supports_primitives_map_:
            iim = _xpcom.XPTI_GetInterfaceInfoManager()
            for (iid_name, attr, cvt) in _supports_primitives_data_:
                special_iid = iim.GetInfoForName(iid_name).GetIID()
                _supports_primitives_map_[special_iid] = (attr, cvt)
        attr, cvt = _supports_primitives_map_.get(iid, (None,None))
        if attr is not None and hasattr(self._obj_, attr):
            return xpcom.server.WrapObject(SupportsPrimitive(iid, self._obj_, attr, cvt), iid, bWrapClient = 0)
        # Out of clever things to try!
        return None # We dont support this IID.

    def _MakeInterfaceParam_(self, interface, iid, method_index, mi, param_index):
        # Wrap a "raw" interface object in a nice object.  The result of this
        # function will be passed to one of the gateway methods.
        if iid is None:
            # look up the interface info - this will be true for all xpcom called interfaces.
            if self._interface_info_ is None:
                import xpcom.xpt
                self._interface_info_ = xpcom.xpt.Interface( self._iid_ )
            iid = self._interface_iid_map_.get( (method_index, param_index))
            if iid is None:
                iid = self._interface_info_.GetIIDForParam(method_index, param_index)
                self._interface_iid_map_[(method_index, param_index)] = iid
        # handle nsIVariant
        if iid == IID_nsIVariant:
            interface = interface.QueryInterface(iid)
            dt = interface.dataType
            if dt in VARIANT_INT_TYPES:
                return interface.getAsInt32()
            if dt in VARIANT_LONG_TYPES:
                return interface.getAsInt64()
            if dt in VARIANT_FLOAT_TYPES:
                return interface.getAsFloat()
            if dt in VARIANT_STRING_TYPES:
                return interface.getAsStringWithSize()
            if dt in VARIANT_UNICODE_TYPES:
                return interface.getAsWStringWithSize()
            if dt == xpcom_consts.VTYPE_BOOL:
                return interface.getAsBool()
            if dt == xpcom_consts.VTYPE_INTERFACE:
                return interface.getAsISupports()
            if dt == xpcom_consts.VTYPE_INTERFACE_IS:
                return interface.getAsInterface()
            if dt == xpcom_consts.VTYPE_EMPTY or dt == xpcom_consts.VTYPE_VOID:
                return None
            if dt == xpcom_consts.VTYPE_ARRAY:
                return interface.getAsArray()
            if dt == xpcom_consts.VTYPE_EMPTY_ARRAY:
                return []
            if dt == xpcom_consts.VTYPE_ID:
                return interface.getAsID()
            # all else fails...
            print "Warning: nsIVariant type %d not supported - returning a string" % (dt,)
            try:
                return interface.getAsString()
            except COMException:
                print "Error: failed to get Variant as a string - returning variant object"
                traceback.print_exc()
                return interface
            
        return client.Component(interface, iid)
    
    def _CallMethod_(self, com_object, index, info, params):
        #print "_CallMethod_", index, info, params
        flags, name, param_descs, ret = info
        assert ret[1][0] == xpcom_consts.TD_UINT32, "Expected an nsresult (%s)" % (ret,)
        if XPT_MD_IS_GETTER(flags):
            # Look for a function of that name
            func = getattr(self._obj_, "get_" + name, None)
            if func is None:
                assert len(param_descs)==1 and len(params)==0, "Can only handle a single [out] arg for a default getter"
                ret = getattr(self._obj_, name) # Let attribute error go here!
            else:
                ret = func(*params)
            return 0, ret
        elif XPT_MD_IS_SETTER(flags):
            # Look for a function of that name
            func = getattr(self._obj_, "set_" + name, None)
            if func is None:
                assert len(param_descs)==1 and len(params)==1, "Can only handle a single [in] arg for a default setter"
                setattr(self._obj_, name, params[0]) # Let attribute error go here!
            else:
                func(*params)
            return 0
        else:
            # A regular method.
            func = getattr(self._obj_, name)
            return 0, func(*params)

    def _doHandleException(self, func_name, exc_info):
        exc_val = exc_info[1]
        is_server_exception = isinstance(exc_val, ServerException)
        if is_server_exception:
            if xpcom.verbose:
                print "** Information:  '%s' raised COM Exception %s" % (func_name, exc_val)
                traceback.print_exception(exc_info[0], exc_val, exc_info[2])
                print "** Returning nsresult from existing exception", exc_val
            return exc_val.errno
        # Unhandled exception - always print a warning.
        print "** Unhandled exception calling '%s'" % (func_name,)
        traceback.print_exception(exc_info[0], exc_val, exc_info[2])
        print "** Returning nsresult of NS_ERROR_FAILURE"
        return nsError.NS_ERROR_FAILURE


    # Called whenever an unhandled Python exception is detected as a result
    # of _CallMethod_ - this exception may have been raised during the _CallMethod_
    # invocation, or after its return, but when unpacking the results
    # eg, type errors, such as a Python integer being used as a string "out" param.
    def _CallMethodException_(self, com_object, index, info, params, exc_info):
        # Later we may want to have some smart "am I debugging" flags?
        # Or maybe just delegate to the actual object - it's probably got the best
        # idea what to do with them!
        flags, name, param_descs, ret = info
        exc_typ, exc_val, exc_tb = exc_info
        # use the xpt module to get a better repr for the method.
        # But if we fail, ignore it!
        try:
            import xpcom.xpt
            m = xpcom.xpt.Method(info, index, None)
            func_repr = m.Describe().lstrip()
        except:
            func_repr = "%s(%r)" % (name, param_descs)
        return self._doHandleException(func_repr, exc_info)

    # Called whenever a gateway fails due to anything other than _CallMethod_.
    # Really only used for the component loader etc objects, so most
    # users should never see exceptions triggered here.
    def _GatewayException_(self, name, exc_info):
        return self._doHandleException(name, exc_info)

_supports_primitives_data_ = [
    ("nsISupportsCString", "__str__", str),
    ("nsISupportsString", "__str__", str),
    ("nsISupportsPRUint64", "__long__", long),
    ("nsISupportsPRInt64", "__long__", long),
    ("nsISupportsPRUint32", "__int__", int),
    ("nsISupportsPRInt32", "__int__", int),
    ("nsISupportsPRUint16", "__int__", int),
    ("nsISupportsPRInt16", "__int__", int),
    ("nsISupportsPRUint8", "__int__", int),
    ("nsISupportsPRBool", "__nonzero__", operator.truth),
    ("nsISupportsDouble", "__float__", float),
    ("nsISupportsFloat", "__float__", float),
]

# Support for the nsISupports primitives:
class SupportsPrimitive:
    _com_interfaces_ = ["nsISupports"]
    def __init__(self, iid, base_ob, attr_name, converter):
        self.iid = iid
        self.base_ob = base_ob
        self.attr_name = attr_name
        self.converter = converter
    def _query_interface_(self, iid):
        if iid == self.iid:
            return 1
        return None
    def get_data(self):
        method = getattr(self.base_ob, self.attr_name)
        val = method()
        return self.converter(val)
    def set_data(self, val):
        raise ServerException(nsError.NS_ERROR_NOT_IMPLEMENTED)
    def toString(self):
        return str(self.get_data())

def _shutdown():
    class_info_cache.clear()
