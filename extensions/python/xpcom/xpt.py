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
#   David Ascher <DavidA@ActiveState.com> (original author)
#   Mark Hammond <mhammond@skippinet.com.au>
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

"""
Program: xpt.py

Task: describe interfaces etc using XPCOM reflection.

Subtasks:
     output (nearly) exactly the same stuff as xpt_dump, for verification
     output Python source code that can be used as a template for an interface

Status: Works pretty well if you ask me :-)

Author:
   David Ascher did an original version that parsed XPT files
   directly.  Mark Hammond changed it to use the reflection interfaces,
   but kept most of the printing logic.


Revision:

  0.1: March 6, 2000
  0.2: April 2000 - Mark removed lots of Davids lovely parsing code in favour
                    of the new xpcom interfaces that provide this info.

  May 2000 - Moved into Perforce - track the log there!
  Early 2001 - Moved into the Mozilla CVS tree - track the log there!  

Todo:
  Fill out this todo list.

"""

import string, sys
import xpcom
import xpcom._xpcom

from xpcom_consts import *

class Interface:
    def __init__(self, iid):
        iim = xpcom._xpcom.XPTI_GetInterfaceInfoManager()
        try:
            if hasattr(iid, "upper"): # Is it a stringy thing.
                item = iim.GetInfoForName(iid)
            else:
                item = iim.GetInfoForIID(iid)
        except xpcom.COMException:
            name = getattr(iid, "name", str(iid))
            print "Failed to get info for IID '%s'" % (name,)
            raise
        self.interface_info = item
        self.namespace = "" # where does this come from?
        self.methods = Methods(item)
        self.constants = Constants(item)

    # delegate attributes to the real interface
    def __getattr__(self, attr):
        return getattr(self.interface_info, attr)

    def GetParent(self):
        try:
            raw_parent = self.interface_info.GetParent()
            if raw_parent is None:
                return None
            return Interface(raw_parent.GetIID())
        except xpcom.Exception:
            # Parent interface is probably not scriptable - assume nsISupports.
            if xpcom.verbose:
                # The user may be confused as to why this is happening!
                print "The parent interface of IID '%s' can not be located - assuming nsISupports"
            return Interface(xpcom._xpcom.IID_nsISupports)

    def Describe_Python(self):
        method_reprs = []
        methods = filter(lambda m: not m.IsNotXPCOM(), self.methods)
        for m in methods:
            method_reprs.append(m.Describe_Python())
        method_joiner = "\n"
        methods_repr = method_joiner.join(method_reprs)
        return \
"""class %s:
    _com_interfaces_ = xpcom.components.interfaces.%s
    # If this object needs to be registered, the following 2 are also needed.
    # _reg_clsid_ = "{a new clsid generated for this object}"
    # _reg_contractid_ = "The.Object.Name"\n%s""" % (self.GetName(), self.GetIID().name, methods_repr)

    def Describe(self):
        # Make the IID look like xtp_dump - "(" instead of "{"
        iid_use = "(" + str(self.GetIID())[1:-1] + ")"
        s = '   - '+self.namespace+'::'+ self.GetName() + ' ' + iid_use + ':\n'

        parent = self.GetParent()
        if parent is not None:
            s = s + '      Parent: ' + parent.namespace + '::' + parent.GetName() + '\n'
        s = s + '      Flags:\n'
        if self.IsScriptable(): word = 'TRUE'
        else: word = 'FALSE'
        s = s + '         Scriptable: ' + word + '\n'
        s = s + '      Methods:\n'
        methods = filter(lambda m: not m.IsNotXPCOM(), self.methods)
        if len(methods):
            for m in methods:
                s = s + '   ' + m.Describe() + '\n'
        else:
            s = s + '         No Methods\n'
        s = s + '      Constants:\n'
        if self.constants:
            for c in self.constants:
                s = s + '         ' + c.Describe() + '\n'
        else:
            s = s + '         No Constants\n'

        return s

# A class that allows caching and iterating of methods.
class Methods:
    def __init__(self, interface_info):
        self.interface_info = interface_info
        try:
            self.items = [None] * interface_info.GetMethodCount()
        except xpcom.Exception:
            if xpcom.verbose:
                print "** GetMethodCount failed?? - assuming no methods"
            self.items = []
    def __len__(self):
        return len(self.items)
    def __getitem__(self, index):
        ret = self.items[index]
        if ret is None:
            mi = self.interface_info.GetMethodInfo(index)
            ret = self.items[index] = Method(mi, index, self.interface_info)
        return ret

class Method:

    def __init__(self, method_info, method_index, interface_info = None):
        self.interface_info = interface_info
        self.method_index = method_index
        self.flags, self.name, param_descs, self.result_desc = method_info
        # Build the params.
        self.params = []
        pi=0
        for pd in param_descs:
            self.params.append( Parameter(pd, pi, method_index, interface_info) )
            pi = pi + 1
        # Run over the params setting the "sizeof" params to hidden.
        for p in self.params:
            td = p.type_desc
            tag = XPT_TDP_TAG(td[0])
            if tag==T_ARRAY and p.IsIn():
                self.params[td[1]].hidden_indicator = 2
            elif tag in [T_PSTRING_SIZE_IS, T_PWSTRING_SIZE_IS] and p.IsIn():
                self.params[td[1]].hidden_indicator = 1

    def IsGetter(self):
        return (self.flags & XPT_MD_GETTER)
    def IsSetter(self):
        return (self.flags & XPT_MD_SETTER)
    def IsNotXPCOM(self):
        return (self.flags & XPT_MD_NOTXPCOM)
    def IsConstructor(self):
        return (self.flags & XPT_MD_CTOR)
    def IsHidden(self):
        return (self.flags & XPT_MD_HIDDEN)

    def Describe_Python(self):
        if self.method_index < 3: # Ignore QI etc
            return ""
        base_name = self.name
        if self.IsGetter():
            name = "get_%s" % (base_name,)
        elif self.IsSetter():
            name = "set_%s" % (base_name,)
        else:
            name = base_name
        param_decls = ["self"]
        in_comments = []
        out_descs = []
        result_comment = "Result: void - None"
        for p in self.params:
            in_desc, in_desc_comments, out_desc, this_result_comment = p.Describe_Python()
            if in_desc is not None:
                param_decls.append(in_desc)
            if in_desc_comments is not None:
                in_comments.append(in_desc_comments)
            if out_desc is not None:
                out_descs.append(out_desc)
            if this_result_comment is not None:
                result_comment = this_result_comment
        joiner = "\n        # "
        in_comment = out_desc = ""
        if in_comments: in_comment = joiner + joiner.join(in_comments)
        if out_descs: out_desc = joiner + joiner.join(out_descs)

        return """    def %s( %s ):
        # %s%s%s
        pass""" % (name, ", ".join(param_decls), result_comment, in_comment, out_desc)

    def Describe(self):
        s = ''
        if self.IsGetter():
            G = 'G'
        else:
            G = ' '
        if self.IsSetter():
            S = 'S'
        else: S = ' '
        if self.IsHidden():
            H = 'H'
        else:
            H = ' '
        if self.IsNotXPCOM():
            N = 'N'
        else:
            N = ' '
        if self.IsConstructor():
            C = 'C'
        else:
            C = ' '

        def desc(a): return a.Describe()
        method_desc = string.join(map(desc, self.params), ', ')
        result_type = TypeDescriber(self.result_desc[0], None)
        return_desc = result_type.Describe()
        i = string.find(return_desc, 'retval ')
        if i != -1:
            return_desc = return_desc[:i] + return_desc[i+len('retval '):]
        return G+S+H+N+C+' '+return_desc+' '+self.name + '('+ method_desc + ');'

class Parameter:
    def __init__(self,  param_desc, param_index, method_index, interface_info = None):
        self.param_flags, self.type_desc = param_desc
        self.hidden_indicator = 0 # Is this a special "size" type param that will be hidden from Python?
        self.param_index = param_index
        self.method_index= method_index
        self.interface_info = interface_info
    def __repr__(self):
        return "<param %(param_index)d (method %(method_index)d) - flags = 0x%(param_flags)x, type = %(type_desc)s>" % self.__dict__
    def IsIn(self):
        return XPT_PD_IS_IN(self.param_flags)
    def IsOut(self):
        return XPT_PD_IS_OUT(self.param_flags)
    def IsInOut(self):
        return self.IsIn() and self.IsOut()
    def IsRetval(self):
        return XPT_PD_IS_RETVAL(self.param_flags)
    def IsShared(self):
        return XPT_PD_IS_SHARED(self.param_flags)
    def IsDipper(self):
        return XPT_PD_IS_DIPPER(self.param_flags)

    def Describe_Python(self):
        name = "param%d" % (self.param_index,)
        if self.hidden_indicator:
            # Could remove the comment - Im trying to tell the user where that param has
            # gone from the signature!
            return None, "%s is a hidden parameter" % (name,), None, None
        t = TypeDescriber(self.type_desc[0], self)
        decl = in_comment = out_comment = result_comment = None
        type_desc = t.Describe()
        if self.IsIn() and not self.IsDipper():
            decl = name
            extra=""
            if self.IsOut():
                extra = "Out"
            in_comment = "In%s: %s: %s" % (extra, name, type_desc)
        elif self.IsOut() or self.IsDipper():
            if self.IsRetval():
                result_comment = "Result: %s" % (type_desc,)
            else:
                out_comment = "Out: %s" % (type_desc,)
        return decl, in_comment, out_comment, result_comment

    def Describe(self):
        parts = []
        if self.IsInOut():
            parts.append('inout')
        elif self.IsIn():
            parts.append('in')
        elif self.IsOut():
            parts.append('out')

        if self.IsDipper(): parts.append("dipper")
        if self.IsRetval(): parts.append('retval')
        if self.IsShared(): parts.append('shared')
        t = TypeDescriber(self.type_desc[0], self)
        type_str = t.Describe()
        parts.append(type_str)
        return string.join(parts)

# A class that allows caching and iterating of constants.
class Constants:
    def __init__(self, interface_info):
        self.interface_info = interface_info
        try:
            self.items = [None] * interface_info.GetConstantCount()
        except xpcom.Exception:
            if xpcom.verbose:
                print "** GetConstantCount failed?? - assuming no constants"
            self.items = []
    def __len__(self):
        return len(self.items)
    def __getitem__(self, index):
        ret = self.items[index]
        if ret is None:
            ci = self.interface_info.GetConstant(index)
            ret = self.items[index] = Constant(ci)
        return ret

class Constant:
    def __init__(self, ci):
        self.name, self.type, self.value = ci

    def Describe(self):
        return TypeDescriber(self.type, None).Describe() + ' ' +self.name+' = '+str(self.value)+';'

    __str__ = Describe

def MakeReprForInvoke(param):
    tag = param.type_desc[0] & XPT_TDP_TAGMASK
    if tag == T_INTERFACE:
        i_info = param.interface_info
        try:
            iid = i_info.GetIIDForParam(param.method_index, param.param_index)
        except xpcom.Exception:
            # IID not available (probably not scriptable) - just use nsISupports.
            iid = xpcom._xpcom.IID_nsISupports
        return param.type_desc[0], 0, 0, str(iid)
    elif tag == T_ARRAY:
        i_info = param.interface_info
        array_desc = i_info.GetTypeForParam(param.method_index, param.param_index, 1)
        return param.type_desc[:-1] + array_desc[:1]
    return param.type_desc


class TypeDescriber:
    def __init__(self, type_flags, param):
        self.type_flags = type_flags
        self.tag = XPT_TDP_TAG(self.type_flags)
        self.param = param
    def IsPointer(self):
        return XPT_TDP_IS_POINTER(self.type_flags)
    def IsUniquePointer(self):
        return XPT_TDP_IS_UNIQUE_POINTER(self.type_flags)
    def IsReference(self):
        return XPT_TDP_IS_REFERENCE(self.type_flags)
    def repr_for_invoke(self):
        return (self.type_flags,)
    def GetName(self):
        is_ptr = self.IsPointer()
        data = type_info_map.get(self.tag)
        if data is None:
            data = ("unknown",)
        if self.IsReference():
            if len(data) > 2:
                return data[2]
            return data[0] + " &"
        if self.IsPointer():
            if len(data)>1:
                return data[1]
            return data[0] + " *"
        return data[0]

    def Describe(self):
        if self.tag == T_ARRAY:
            # NOTE - Adding a type specifier to the array is different from xpt_dump.exe
            if self.param is None or self.param.interface_info is None:
                type_desc = "" # Dont have explicit info about the array type :-(
            else:
                i_info = self.param.interface_info
                type_code = i_info.GetTypeForParam(self.param.method_index, self.param.param_index, 1)
                type_desc = TypeDescriber( type_code[0], None).Describe()
            return self.GetName() + "[" + type_desc + "]" 
        elif self.tag == T_INTERFACE:
            if self.param is None or self.param.interface_info is None:
                return "nsISomething" # Dont have explicit info about the IID :-(
            i_info = self.param.interface_info
            m_index = self.param.method_index
            p_index = self.param.param_index
            try:
                iid = i_info.GetIIDForParam(m_index, p_index)
                return iid.name
            except xpcom.Exception:
                return "nsISomething"
        return self.GetName()

# These are just for output purposes, so should be
# the same as xpt_dump uses
type_info_map = {
    T_I8                : ("int8",),
    T_I16               : ("int16",),
    T_I32               : ("int32",),
    T_I64               : ("int64",),
    T_U8                : ("uint8",),
    T_U16               : ("uint16",),
    T_U32               : ("uint32",),
    T_U64               : ("uint64",),
    T_FLOAT             : ("float",),
    T_DOUBLE            : ("double",),
    T_BOOL              : ("boolean",),
    T_CHAR              : ("char",),
    T_WCHAR             : ("wchar_t", "wstring"),
    T_VOID              : ("void",),
    T_IID               : ("reserved", "nsIID *", "nsIID &"),
    T_DOMSTRING         : ("DOMString",),
    T_CHAR_STR          : ("reserved", "string"),
    T_WCHAR_STR         : ("reserved", "wstring"),
    T_INTERFACE         : ("reserved", "Interface"),
    T_INTERFACE_IS      : ("reserved", "InterfaceIs *"),
    T_ARRAY             : ("reserved", "Array"),
    T_PSTRING_SIZE_IS   : ("reserved", "string_s"),
    T_PWSTRING_SIZE_IS  : ("reserved", "wstring_s"),
}

def dump_interface(iid, mode):
    interface = Interface(iid)
    describer_name = "Describe"
    if mode == "xptinfo": mode = None
    if mode is not None:
        describer_name = describer_name + "_" + mode.capitalize()
    describer = getattr(interface, describer_name)
    print describer()

if __name__=='__main__':
    if len(sys.argv) == 1:
        print "Usage: xpt.py [-xptinfo] interface_name, ..."
        print "  -info: Dump in a style similar to the xptdump tool"
        print "Dumping nsISupports and nsIInterfaceInfo"
        sys.argv.append('nsIInterfaceInfo')
        sys.argv.append('-xptinfo')
        sys.argv.append('nsISupports')
        sys.argv.append('nsIInterfaceInfo')

    mode = "Python"
    for i in sys.argv[1:]:
        if i[0] == "-":
            mode = i[1:]
        else:
            dump_interface(i, mode)
