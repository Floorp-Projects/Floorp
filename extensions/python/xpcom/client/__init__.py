# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

import os
import new
from xpcom import xpt, _xpcom, COMException, nsError

XPTC_InvokeByIndex = _xpcom.XPTC_InvokeByIndex

_just_int_interfaces = ["nsISupportsPRInt32", "nsISupportsPRInt16", "nsISupportsPRUint32", "nsISupportsPRUint16", "nsISupportsPRUint8", "nsISupportsPRBool"]
_just_long_interfaces = ["nsISupportsPRInt64", "nsISupportsPRUint64"]
_just_float_interfaces = ["nsISupportsDouble", "nsISupportsFloat"]
# When doing a specific conversion, the order we try the interfaces in.
_int_interfaces = _just_int_interfaces + _just_float_interfaces
_long_interfaces = _just_long_interfaces + _just_int_interfaces + _just_float_interfaces
_float_interfaces = _just_float_interfaces + _just_long_interfaces + _just_int_interfaces

method_template = """
def %s(self, %s):
    return XPTC_InvokeByIndex(self._comobj_, %d,
                              (%s,
                               (%s)))
"""
def _MakeMethodCode(method):
    # Build a declaration
    param_no = 0
    param_decls = []
    param_flags = []
    param_names = []
    used_default = 0
    for param in method.params:
        param_no = param_no + 1
        param_name = "Param%d" % (param_no,)
        param_default = ""
        if not param.hidden_indicator and param.IsIn() and not param.IsDipper():
            if param.IsOut() or used_default: # If the param is "inout", provide a useful default for the "in" direction.
                param_default = " = None"
                used_default = 1 # Once we have used one once, we must for the rest!
            param_decls.append(param_name + param_default)
            param_names.append(param_name)

        type_repr = xpt.MakeReprForInvoke(param)
        param_flags.append( (param.param_flags,) +  type_repr )
    sep = ", "
    param_decls = sep.join(param_decls)
    if len(param_names)==1: # Damn tuple reprs.
        param_names = param_names[0] + ","
    else:
        param_names = sep.join(param_names)
    # A couple of extra newlines make them easier to read for debugging :-)
    return method_template % (method.name, param_decls, method.method_index, tuple(param_flags), param_names)

# Keyed by IID, each item is a tuple of (methods, getters, setters)
interface_cache = {}

# Fully process the interface - generate method code, etc.
def BuildInterfaceInfo(iid):
    ret = interface_cache.get(iid, None)
    if ret is None:
        # Build the data for the cache.
        method_code_blocks = []
        getters = {}
        setters = {}
        method_names = []
        
        interface = xpt.Interface(iid)
        for m in interface.methods:
            if not m.IsNotXPCOM() and \
                     not m.IsHidden() and \
                     not m.IsConstructor():
                # Yay - a method we can use!
                if m.IsSetter():
                    param_flags = map(lambda x: (x.param_flags,) + xpt.MakeReprForInvoke(x), m.params)
                    setters[m.name] = m.method_index, param_flags
                elif m.IsGetter():
                    param_flags = map(lambda x: (x.param_flags,) + xpt.MakeReprForInvoke(x), m.params)
                    getters[m.name] = m.method_index, param_flags
                else:
                    method_names.append(m.name)
                    method_code_blocks.append(_MakeMethodCode(m))

        # Build the constants.
        constants = {}
        for c in interface.constants:
            constants[c.name] = c.value
        # Build the methods - We only build function objects here
        # - they are bound to each instance at instance creation.
        methods = {}
        if len(method_code_blocks)!=0:
            method_code = "\n".join(method_code_blocks)
##            print "Method Code:"
##            print method_code
            codeObject = compile(method_code, "<XPCOMObject method>","exec")
            # Exec the code object
            tempNameSpace = {}
            exec codeObject in globals(), tempNameSpace # self.__dict__, self.__dict__
            for name in method_names:
                methods[name] = tempNameSpace[name]
        ret = methods, getters, setters, constants
        interface_cache[iid] = ret
    return ret

def Component(ob, iid):
    ob_name = None
    if not hasattr(ob, "IID"):
        ob_name = ob
        cm = _xpcom.NS_GetGlobalComponentManager()
        ob = cm.CreateInstanceByContractID(ob)
    return Interface(ob, iid)

class Interface:
    """Implements a dynamic interface using xpcom reflection.
    """
    def __init__(self, ob, iid, object_name = None):
        ob = ob.QueryInterface(iid, 0) # zero means "no auto-wrap"
        self.__dict__['_comobj_'] = ob
        # Hack - the last interface only!
        methods, getters, setters, constants = BuildInterfaceInfo(iid)
        self.__dict__['_interface_infos_'] = getters, setters
        self.__dict__['_interface_methods_'] = methods # Unbound methods.

        self.__dict__.update(constants)
        # We remember the constant names to prevent the user trying to assign to them!
        self.__dict__['_constant_names_'] = constants.keys()

        if object_name is None:
            object_name = "object with interface '%s'" % (iid.name,)
        self.__dict__['_object_name_'] = object_name

    def __cmp__(self, other):
        try:
            other = other._comobj_
        except AttributeError:
            pass
        return cmp(self._comobj_, other)

    def __hash__(self):
        return hash(self._comobj_)

    def __repr__(self):
        return "<XPCOM interface '%s'>" % (self._comobj_.IID.name,)

    # See if the object support strings.
    def __str__(self):
        try:
            self._comobj_.QueryInterface(_xpcom.IID_nsISupportsString)
            return str(self._comobj_)
        except COMException:
            return self.__repr__()

    # Try the numeric support.
    def _do_conversion(self, interface_names, cvt):
        iim = _xpcom.XPTI_GetInterfaceInfoManager()
        for interface_name in interface_names:
            iid = iim.GetInfoForName(interface_name).GetIID()
            try:
                prim = self._comobj_.QueryInterface(iid)
                return cvt(prim.data)
            except COMException:
                pass
        raise ValueError, "This object does not support automatic numeric conversion to this type"

    def __int__(self):
        return self._do_conversion(_int_interfaces, int)

    def __long__(self):
        return self._do_conversion(_long_interfaces, long)

    def __float__(self):
        return self._do_conversion(_float_interfaces, float)

    def __getattr__(self, attr):
        # Allow the underlying interface to provide a better implementation if desired.
        ret = getattr(self.__dict__['_comobj_'], attr, None)
        if ret is not None:
            return ret
        # Do the function thing first.
        unbound_method = self.__dict__['_interface_methods_'].get(attr)
        if unbound_method is not None:
            return new.instancemethod(unbound_method, self, self.__class__)

        getters, setters = self.__dict__['_interface_infos_']
        info = getters.get(attr)
        if info is None:
            raise AttributeError, "XPCOM component '%s' has no attribute '%s'" % (self._object_name_, attr)
        method_index, param_infos = info
        if len(param_infos)!=1: # Only expecting a retval
            raise RuntimeError, "Can't get properties with this many args!"
        args = ( param_infos, () )
        return XPTC_InvokeByIndex(self._comobj_, method_index, args)

    def __setattr__(self, attr, val):
        # If we already have a __dict__ item of that name, and its not one of
        # our constants, we just directly set it, and leave.
        if self.__dict__.has_key(attr) and attr not in self.__dict__['_constant_names_']:
            self.__dict__[attr] = val
            return
        # Start sniffing for what sort of attribute this might be?
        getters, setters = self.__dict__['_interface_infos_']
        info = setters.get(attr)
        if info is None:
            raise AttributeError, "XPCOM component '%s' can not set attribute '%s'" % (self._object_name_, attr)
        method_index, param_infos = info
        if len(param_infos)!=1: # Only expecting a single input val
            raise RuntimeError, "Can't set properties with this many args!"
        real_param_infos = ( param_infos, (val,) )
        return XPTC_InvokeByIndex(self._comobj_, method_index, real_param_infos)

# Called by the _xpcom C++ framework to wrap interfaces up just
# before they are returned.
def MakeInterfaceResult(ob, iid):
    return Interface(ob, iid)

class WeakReference:
    """A weak-reference object.  You construct a weak reference by passing
    any COM object you like.  If the object does not support weak
    refs, you will get a standard NS_NOINTERFACE exception.

    Once you have a weak-reference, you can "call" the object to get
    back a strong reference.  Eg:

    >>> some_ob = components.classes['...]
    >>> weak_ref = WeakReference(some_ob)
    >>> new_ob = weak_ref() # new_ob is effectively "some_ob" at this point
    >>> # EXCEPT: new_ob may be None of some_ob has already died - a
    >>> # weak reference does not keep the object alive (that is the point)

    You should never hold onto this resulting strong object for a long time,
    or else you defeat the purpose of the weak-reference.
    """
    def __init__(self, ob, iid = None):
        swr = Interface(ob, _xpcom.IID_nsISupportsWeakReference)
        self._comobj_ = Interface(swr.GetWeakReference(), _xpcom.IID_nsIWeakReference)
        if iid is None:
            try:
                iid = ob.IID
            except AttributeError:
                iid = _xpcom.IID_nsISupports
        self._iid_ = iid
    def __call__(self, iid = None):
        if iid is None: iid = self._iid_
        try:
            return Interface(self._comobj_.QueryReferent(iid), iid)
        except COMException, details:
            if details.errno != nsError.NS_ERROR_NULL_POINTER:
                raise
            return None
