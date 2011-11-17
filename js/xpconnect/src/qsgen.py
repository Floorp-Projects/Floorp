#!/usr/bin/env/python
# qsgen.py - Generate XPConnect quick stubs.
#
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
#   Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jason Orendorff <jorendorff@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

# =About quick stubs=
# qsgen.py generates "quick stubs", custom SpiderMonkey getters, setters, and
# methods for specified XPCOM interface members.  These quick stubs serve at
# runtime as replacements for the XPConnect functions XPC_WN_GetterSetter and
# XPC_WN_CallMethod, which are the extremely generic (and slow) SpiderMonkey
# getter/setter/methods otherwise used for all XPCOM member accesses from JS.
#
# There are two ways quick stubs win:
#   1. Pure, transparent optimization by partial evaluation.
#   2. Cutting corners.
#
# == Partial evaluation ==
# Partial evaluation is when you execute part of a program early (before or at
# compile time) so that you don't have to execute it at run time.  In this
# case, everything that involves interpreting xptcall data (for example, the
# big methodInfo loops in XPCWrappedNative::CallMethod and the switch statement
# in XPCConert::JSData2Native) might as well happen at build time, since all
# the type information for any given member is already known.  That's what this
# script does.  It gets the information from IDL instead of XPT files.  Apart
# from that, the code in this script is very similar to what you'll find in
# XPConnect itself.  The advantage is that it runs once, at build time, not in
# tight loops at run time.
#
# == Cutting corners ==
# The XPConnect versions have to be slow because they do tons of work that's
# only necessary in a few cases.  The quick stubs skip a lot of that work.  So
# quick stubs necessarily differ from XPConnect in potentially observable ways.
# For many specific interface members, the differences are not observable from
# scripts or don't matter enough to worry about; but you do have to be careful
# which members you decide to generate quick stubs for.
#
# The complete list of known differences, as of this writing, after an
# assiduous search:
#
# - Quick stubs affect the handling of naming conflicts--that is, which C++
#   method gets called when a script uses an XPCOM feature that is declared in
#   more than one of the interfaces the object implements.  Without quick
#   stubs, XPConnect just walks the interfaces in the order they're listed by
#   nsClassInfo.  You get the first interface that implements a feature with
#   that name.  With quick stubs, it's the same except that non-quick-stubbed
#   features are shadowed.
#
# - Quick stub methods are JSFastNative, which means that when a quick stub
#   method is called, no JS stack frame is created.  This doesn't affect
#   Mozilla security checks because they look for scripted JSStackFrames, not
#   native ones.
#
#   It does affect the 'stack' property of JavaScript exceptions, though: the
#   stubbed member will not appear.  (Note that if the stubbed member itself
#   fails, the member name will appear in the 'message' property.)
#
# - Many quick stubs don't create an XPCCallContext.  In those cases, no entry
#   is added to the XPCCallContext stack.  So native implementations of
#   quick-stubbed methods must avoid nsXPConnect::GetCurrentNativeCallContext.
#
#   (Even when a quick stub does have an XPCCallContext, it never pushes it all
#   the way to READY_TO_CALL state, so a lot of its members are garbage.  But
#   this doesn't endanger native implementations of non-quick-stubbed methods
#   that use GetCurrentNativeCallContext and are called indirectly from
#   quick-stubbed methods, because only the current top XPCCallContext is
#   exposed--nsAXPCNativeCallContext does not expose
#   XPCCallContext::GetPrevCallContext.)
#
# - Quick stubs never suspend the JS request.  So they are only suitable for
#   main-thread-only interfaces.
#
# - Quick stubs don't call XPCContext::SetLastResult.  This is visible on the
#   Components object.
#
# - Quick stubs skip a security check that XPConnect does in
#   XPCWrappedNative::CallMethod.  This means the security manager doesn't have
#   an opportunity to veto accesses to members for which quick stubs exist.
#
# - There are many features of IDL that XPConnect supports but qsgen does not,
#   including dependent types, arrays, and out parameters.
#
# - Since quick stubs are JSPropertyOps, we have to do additional work to make
#   __lookup[GS]etter__ work on them.


import xpidl
import header
import os, re
import sys

# === Preliminaries

MAX_TRACEABLE_NATIVE_ARGS = 8

# --makedepend-output support.
make_dependencies = []
make_targets = []

def warn(msg):
    sys.stderr.write(msg + '\n')

def unaliasType(t):
    while t.kind == 'typedef':
        t = t.realtype
    assert t is not None
    return t

def isVoidType(type):
    """ Return True if the given xpidl type is void. """
    return type.kind == 'builtin' and type.name == 'void'

def isInterfaceType(t):
    t = unaliasType(t)
    assert t.kind in ('builtin', 'native', 'interface', 'forward')
    return t.kind in ('interface', 'forward')

def isSpecificInterfaceType(t, name):
    """ True if `t` is an interface type with the given name, or a forward
    declaration or typedef aliasing it.

    `name` must not be the name of a typedef but the actual name of the
    interface.
    """
    t = unaliasType(t)
    return t.kind in ('interface', 'forward') and t.name == name

def getBuiltinOrNativeTypeName(t):
    t = unaliasType(t)
    if t.kind == 'builtin':
        return t.name
    elif t.kind == 'native':
        assert t.specialtype is not None
        return '[%s]' % t.specialtype
    else:
        return None


# === Reading the file

class UserError(Exception):
    pass

def findIDL(includePath, irregularFilenames, interfaceName):
    filename = irregularFilenames.get(interfaceName, interfaceName) + '.idl'
    for d in includePath:
        # Not os.path.join: we need a forward slash even on Windows because
        # this filename ends up in makedepend output.
        path = d + '/' + filename
        if os.path.exists(path):
            return path
    raise UserError("No IDL file found for interface %s "
                    "in include path %r"
                    % (interfaceName, includePath))

def loadIDL(parser, includePath, filename):
    make_dependencies.append(filename)
    text = open(filename, 'r').read()
    idl = parser.parse(text, filename=filename)
    idl.resolve(includePath, parser)
    return idl

def removeStubMember(memberId, member):
    if member not in member.iface.stubMembers:
        raise UserError("Trying to remove member %s from interface %s, but it was never added"
                        % (member.name, member.iface.name))
    member.iface.stubMembers.remove(member)

def addStubMember(memberId, member, traceable):
    mayTrace = False
    if member.kind == 'method' and not member.implicit_jscontext and not isVariantType(member.realtype):
        # This code MUST match writeTraceableQuickStub
        haveCallee = memberNeedsCallee(member)
        # Traceable natives support up to MAX_TRACEABLE_NATIVE_ARGS
        # total arguments.  We always have two prefix arguments
        # (CONTEXT and THIS) and when haveCallee is true also have
        # CALLEE.  We can only output a traceable native if our number
        # of arguments is no bigger than can be handled.
        prefixArgCount = 2 + int(haveCallee);
        mayTrace = (len(member.params) <= MAX_TRACEABLE_NATIVE_ARGS - prefixArgCount)

        for param in member.params:
            for attrname, value in vars(param).items():
                if value is True:
                    # Let the tracer call the regular fastnative method if there
                    # are optional arguments.
                    if attrname == 'optional':
                        mayTrace = False
                        continue

                    raise UserError("Method %s, parameter %s: "
                                    "unrecognized property %r"
                                    % (memberId, param.name, attrname))

    member.traceable = traceable and mayTrace

    # Add this member to the list.
    member.iface.stubMembers.append(member)

def checkStubMember(member, isCustom):
    memberId = member.iface.name + "." + member.name
    if member.kind not in ('method', 'attribute'):
        raise UserError("Member %s is %r, not a method or attribute."
                        % (memberId, member.kind))
    if member.noscript:
        raise UserError("%s %s is noscript."
                        % (member.kind.capitalize(), memberId))
    if member.notxpcom:
        raise UserError(
            "%s %s: notxpcom methods are not supported."
            % (member.kind.capitalize(), memberId))

    if (member.kind == 'attribute'
          and not member.readonly
          and isSpecificInterfaceType(member.realtype, 'nsIVariant')
          and not isCustom):
        raise UserError(
            "Attribute %s: Non-readonly attributes of type nsIVariant "
            "are not supported."
            % memberId)

    # Check for unknown properties.
    for attrname, value in vars(member).items():
        if value is True and attrname not in ('readonly','optional_argc',
                                              'traceable','implicit_jscontext',
                                              'getter', 'stringifier'):
            raise UserError("%s %s: unrecognized property %r"
                            % (member.kind.capitalize(), memberId,
                               attrname))

def parseMemberId(memberId):
    """ Split the geven member id into its parts. """
    pieces = memberId.split('.')
    if len(pieces) < 2:
        raise UserError("Member %r: Missing dot." % memberId)
    if len(pieces) > 2:
        raise UserError("Member %r: Dots out of control." % memberId)
    return tuple(pieces)

class Configuration:
    def __init__(self, filename, includePath):
        self.includePath = includePath
        config = {}
        execfile(filename, config)
        # required settings
        for name in ('name', 'members'):
            if name not in config:
                raise UserError(filename + ": `%s` was not defined." % name)
            setattr(self, name, config[name])
        # optional settings
        self.irregularFilenames = config.get('irregularFilenames', {})
        self.customIncludes = config.get('customIncludes', [])
        self.customQuickStubs = config.get('customQuickStubs', [])
        self.customReturnInterfaces = config.get('customReturnInterfaces', [])
        self.customMethodCalls = config.get('customMethodCalls', {})

def readConfigFile(filename, includePath, cachedir, traceable):
    # Read the config file.
    conf = Configuration(filename, includePath)

    # Now read IDL files to connect the information in the config file to
    # actual XPCOM interfaces, methods, and attributes.
    interfaces = []
    interfacesByName = {}
    parser = xpidl.IDLParser(cachedir)

    def getInterface(interfaceName, errorLoc):
        iface = interfacesByName.get(interfaceName)
        if iface is None:
            idlFile = findIDL(conf.includePath, conf.irregularFilenames,
                              interfaceName)
            idl = loadIDL(parser, conf.includePath, idlFile)
            if not idl.hasName(interfaceName):
                raise UserError("The interface %s was not found "
                                "in the idl file %r."
                                % (interfaceName, idlFile))
            iface = idl.getName(interfaceName, errorLoc)
            iface.stubMembers = []
            interfaces.append(iface)
            interfacesByName[interfaceName] = iface
        return iface

    stubbedInterfaces = []

    for memberId in conf.members:
        add = True
        interfaceName, memberName = parseMemberId(memberId)

        # If the interfaceName starts with -, then remove this entry from the list
        if interfaceName[0] == '-':
            add = False
            interfaceName = interfaceName[1:]

        iface = getInterface(interfaceName, errorLoc='looking for %r' % memberId)

        if not iface.attributes.scriptable:
            raise UserError("Interface %s is not scriptable. "
                            "IDL file: %r." % (interfaceName, idlFile))

        if memberName == '*':
            if not add:
                raise UserError("Can't use negation in stub list with wildcard, in %s.*" % interfaceName)

            # Stub all scriptable members of this interface.
            for member in iface.members:
                if member.kind in ('method', 'attribute') and not member.noscript:
                    cmc = conf.customMethodCalls.get(interfaceName + "_" + header.methodNativeName(member), None)
                    mayTrace = cmc is None or cmc.get('traceable', True)

                    addStubMember(iface.name + '.' + member.name, member,
                                  traceable and mayTrace)

                    if member.iface not in stubbedInterfaces:
                        stubbedInterfaces.append(member.iface)
        else:
            # Look up a member by name.
            if memberName not in iface.namemap:
                idlFile = iface.idl.parser.lexer.filename
                raise UserError("Interface %s has no member %r. "
                                "(See IDL file %r.)"
                                % (interfaceName, memberName, idlFile))
            member = iface.namemap.get(memberName, None)
            if add:
                if member in iface.stubMembers:
                    raise UserError("Member %s is specified more than once."
                                    % memberId)

                cmc = conf.customMethodCalls.get(interfaceName + "_" + header.methodNativeName(member), None)
                mayTrace = cmc is None or cmc.get('traceable', True)

                addStubMember(memberId, member, traceable and mayTrace)
                if member.iface not in stubbedInterfaces:
                    stubbedInterfaces.append(member.iface)
            else:
                removeStubMember(memberId, member)

    # Now go through and check all the interfaces' members
    for iface in stubbedInterfaces:
        for member in iface.stubMembers:
            cmc = conf.customMethodCalls.get(iface.name + "_" + header.methodNativeName(member), None)
            skipgen = cmc is not None and cmc.get('skipgen', False)
            checkStubMember(member, skipgen)

    for iface in conf.customReturnInterfaces:
        # just ensure that it exists so that we can grab it later
        iface = getInterface(iface, errorLoc='looking for %s' % (iface,))

    return conf, interfaces


# === Generating the header file

def writeHeaderFile(filename, name):
    print "Creating header file", filename

    headerMacro = '__gen_%s__' % filename.replace('.', '_')
    f = open(filename, 'w')
    try:
        f.write("/* THIS FILE IS AUTOGENERATED - DO NOT EDIT */\n"
                "#ifndef " + headerMacro + "\n"
                "#define " + headerMacro + "\n\n"
                "JSBool " + name + "_DefineQuickStubs("
                "JSContext *cx, JSObject *proto, uintN flags, "
                "PRUint32 count, const nsID **iids);\n\n"
                "void " + name + "_MarkInterfaces();\n\n"
                "void " + name + "_ClearInterfaces();\n\n"
                "inline void " + name + "_InitInterfaces()\n"
                "{\n"
                "  " + name + "_ClearInterfaces();\n"
                "}\n\n"
                "#endif\n")
    finally:
        f.close()

# === Generating the source file

def substitute(template, vals):
    """ Simple replacement for string.Template, which isn't in Python 2.3. """
    def replacement(match):
        return vals[match.group(1)]
    return re.sub(r'\${(\w+)}', replacement, template)

# From JSData2Native.
argumentUnboxingTemplates = {
    'octet':
        "    uint32 ${name}_u32;\n"
        "    if (!JS_ValueToECMAUint32(cx, ${argVal}, &${name}_u32))\n"
        "        return JS_FALSE;\n"
        "    uint8 ${name} = (uint8) ${name}_u32;\n",

    'short':
        "    int32 ${name}_i32;\n"
        "    if (!JS_ValueToECMAInt32(cx, ${argVal}, &${name}_i32))\n"
        "        return JS_FALSE;\n"
        "    int16 ${name} = (int16) ${name}_i32;\n",

    'unsigned short':
        "    uint32 ${name}_u32;\n"
        "    if (!JS_ValueToECMAUint32(cx, ${argVal}, &${name}_u32))\n"
        "        return JS_FALSE;\n"
        "    uint16 ${name} = (uint16) ${name}_u32;\n",

    'long':
        "    int32 ${name};\n"
        "    if (!JS_ValueToECMAInt32(cx, ${argVal}, &${name}))\n"
        "        return JS_FALSE;\n",

    'unsigned long':
        "    uint32 ${name};\n"
        "    if (!JS_ValueToECMAUint32(cx, ${argVal}, &${name}))\n"
        "        return JS_FALSE;\n",

    'long long':
        "    PRInt64 ${name};\n"
        "    if (!xpc_qsValueToInt64(cx, ${argVal}, &${name}))\n"
        "        return JS_FALSE;\n",

    'unsigned long long':
        "    PRUint64 ${name};\n"
        "    if (!xpc_qsValueToUint64(cx, ${argVal}, &${name}))\n"
        "        return JS_FALSE;\n",

    'float':
        "    jsdouble ${name}_dbl;\n"
        "    if (!JS_ValueToNumber(cx, ${argVal}, &${name}_dbl))\n"
        "        return JS_FALSE;\n"
        "    float ${name} = (float) ${name}_dbl;\n",

    'double':
        "    jsdouble ${name};\n"
        "    if (!JS_ValueToNumber(cx, ${argVal}, &${name}))\n"
        "        return JS_FALSE;\n",

    'boolean':
        "    JSBool ${name};\n"
        "    JS_ValueToBoolean(cx, ${argVal}, &${name});\n",

    '[astring]':
        "    xpc_qsAString ${name}(cx, ${argVal}, ${argPtr});\n"
        "    if (!${name}.IsValid())\n"
        "        return JS_FALSE;\n",

    '[domstring]':
        "    xpc_qsDOMString ${name}(cx, ${argVal}, ${argPtr},\n"
        "                            xpc_qsDOMString::e${nullBehavior},\n"
        "                            xpc_qsDOMString::e${undefinedBehavior});\n"
        "    if (!${name}.IsValid())\n"
        "        return JS_FALSE;\n",

    'string':
        "    JSAutoByteString ${name}_bytes;\n"
        "    if (!xpc_qsJsvalToCharStr(cx, ${argVal}, &${name}_bytes))\n"
        "        return JS_FALSE;\n"
        "    char *${name} = ${name}_bytes.ptr();\n",

    'wstring':
        "    const PRUnichar *${name};\n"
        "    if (!xpc_qsJsvalToWcharStr(cx, ${argVal}, ${argPtr}, &${name}))\n"
        "        return JS_FALSE;\n",

    '[cstring]':
        "    xpc_qsACString ${name}(cx, ${argVal}, ${argPtr});\n"
        "    if (!${name}.IsValid())\n"
        "        return JS_FALSE;\n",

    '[utf8string]':
        "    xpc_qsAUTF8String ${name}(cx, ${argVal}, ${argPtr});\n"
        "    if (!${name}.IsValid())\n"
        "        return JS_FALSE;\n",

    '[jsval]':
        "    jsval ${name} = ${argVal};\n"
    }

# From JSData2Native.
#
# Omitted optional arguments are treated as though the caller had passed JS
# `null`; this behavior is from XPCWrappedNative::CallMethod. The 'jsval' type,
# however, defaults to 'undefined'.
#
def writeArgumentUnboxing(f, i, name, type, haveCcx, optional, rvdeclared,
                          nullBehavior, undefinedBehavior):
    # f - file to write to
    # i - int or None - Indicates the source jsval.  If i is an int, the source
    #     jsval is argv[i]; otherwise it is *vp.  But if Python i >= C++ argc,
    #     which can only happen if optional is True, the argument is missing;
    #     use JSVAL_NULL as the source jsval instead.
    # name - str - name of the native C++ variable to create.
    # type - xpidl.{Interface,Native,Builtin} - IDL type of argument
    # optional - bool - True if the parameter is optional.
    # rvdeclared - bool - False if no |nsresult rv| has been declared earlier.

    typeName = getBuiltinOrNativeTypeName(type)

    isSetter = (i is None)

    if isSetter:
        argPtr = "vp"
        argVal = "*vp"
    elif optional:
        if typeName == "[jsval]":
            val = "JSVAL_VOID"
        else:
            val = "JSVAL_NULL"
        argVal = "(%d < argc ? argv[%d] : %s)" % (i, i, val)
        argPtr = "(%d < argc ? &argv[%d] : NULL)" % (i, i)
    else:
        argVal = "argv[%d]" % i
        argPtr = "&" + argVal

    params = {
        'name': name,
        'argVal': argVal,
        'argPtr': argPtr,
        'nullBehavior': nullBehavior or 'DefaultNullBehavior',
        'undefinedBehavior': undefinedBehavior or 'DefaultUndefinedBehavior'
        }

    if typeName is not None:
        template = argumentUnboxingTemplates.get(typeName)
        if template is not None:
            f.write(substitute(template, params))
            return rvdeclared
        # else fall through; the type isn't supported yet.
    elif isInterfaceType(type):
        if type.name == 'nsIVariant':
            # Totally custom.
            assert haveCcx
            template = (
                "    nsCOMPtr<nsIVariant> ${name}(already_AddRefed<nsIVariant>("
                "XPCVariant::newVariant(ccx, ${argVal})));\n"
                "    if (!${name}) {\n"
                "        xpc_qsThrowBadArgWithCcx(ccx, NS_ERROR_XPC_BAD_CONVERT_JS, %d);\n"
                "        return JS_FALSE;\n"
                "    }") % i
            f.write(substitute(template, params))
            return rvdeclared
        elif type.name == 'nsIAtom':
            # Should have special atomizing behavior.  Fall through.
            pass
        else:
            if not rvdeclared:
                f.write("    nsresult rv;\n");
            f.write("    %s *%s;\n" % (type.name, name))
            f.write("    xpc_qsSelfRef %sref;\n" % name)
            f.write("    rv = xpc_qsUnwrapArg<%s>("
                    "cx, %s, &%s, &%sref.ptr, %s);\n"
                    % (type.name, argVal, name, name, argPtr))
            f.write("    if (NS_FAILED(rv)) {\n")
            if isSetter:
                f.write("        xpc_qsThrowBadSetterValue("
                        "cx, rv, JSVAL_TO_OBJECT(*tvr.jsval_addr()), id);\n")
            elif haveCcx:
                f.write("        xpc_qsThrowBadArgWithCcx(ccx, rv, %d);\n" % i)
            else:
                f.write("        xpc_qsThrowBadArg(cx, rv, vp, %d);\n" % i)
            f.write("        return JS_FALSE;\n"
                    "    }\n")
            return True

    warn("Unable to unbox argument of type %s (native type %s)" % (type.name, typeName))
    if i is None:
        src = '*vp'
    else:
        src = 'argv[%d]' % i
    f.write("    !; // TODO - Unbox argument %s = %s\n" % (name, src))
    return rvdeclared

def writeResultDecl(f, type, varname):
    if isVoidType(type):
        return  # nothing to declare

    t = unaliasType(type)
    if t.kind == 'builtin':
        if not t.nativename.endswith('*'):
            if type.kind == 'typedef':
                typeName = type.name  # use it
            else:
                typeName = t.nativename
            f.write("    %s %s;\n" % (typeName, varname))
            return
    elif t.kind == 'native':
        name = getBuiltinOrNativeTypeName(t)
        if name in ('[domstring]', '[astring]'):
            f.write("    nsString %s;\n" % varname)
            return
        elif name == '[jsval]':
            return  # nothing to declare; see special case in outParamForm
    elif t.kind in ('interface', 'forward'):
        f.write("    nsCOMPtr<%s> %s;\n" % (type.name, varname))
        return

    warn("Unable to declare result of type %s" % type.name)
    f.write("    !; // TODO - Declare out parameter `%s`.\n" % varname)

def outParamForm(name, type):
    type = unaliasType(type)
    # If we start allowing [jsval] return types here, we need to tack
    # the return value onto the arguments list in the callers,
    # possibly, and handle properly returning it too.  See bug 604198.
    assert getBuiltinOrNativeTypeName(type) is not '[jsval]'
    if type.kind == 'builtin':
        return '&' + name
    elif type.kind == 'native':
        if getBuiltinOrNativeTypeName(type) == '[jsval]':
            return 'vp'
        elif type.modifier == 'ref':
            return name
        else:
            return '&' + name
    else:
        return 'getter_AddRefs(%s)' % name

# From NativeData2JS.
resultConvTemplates = {
    'void':
            "    ${jsvalRef} = JSVAL_VOID;\n"
            "    return JS_TRUE;\n",

    'octet':
        "    ${jsvalRef} = INT_TO_JSVAL((int32) result);\n"
        "    return JS_TRUE;\n",

    'short':
        "    ${jsvalRef} = INT_TO_JSVAL((int32) result);\n"
        "    return JS_TRUE;\n",

    'long':
        "    return xpc_qsInt32ToJsval(cx, result, ${jsvalPtr});\n",

    'long long':
        "    return xpc_qsInt64ToJsval(cx, result, ${jsvalPtr});\n",

    'unsigned short':
        "    ${jsvalRef} = INT_TO_JSVAL((int32) result);\n"
        "    return JS_TRUE;\n",

    'unsigned long':
        "    return xpc_qsUint32ToJsval(cx, result, ${jsvalPtr});\n",

    'unsigned long long':
        "    return xpc_qsUint64ToJsval(cx, result, ${jsvalPtr});\n",

    'float':
        "    return JS_NewNumberValue(cx, result, ${jsvalPtr});\n",

    'double':
        "    return JS_NewNumberValue(cx, result, ${jsvalPtr});\n",

    'boolean':
        "    ${jsvalRef} = (result ? JSVAL_TRUE : JSVAL_FALSE);\n"
        "    return JS_TRUE;\n",

    '[astring]':
        "    return xpc_qsStringToJsval(cx, result, ${jsvalPtr});\n",

    '[domstring]':
        "    return xpc_qsStringToJsval(cx, result, ${jsvalPtr});\n",

    '[jsval]':
        # Here there's nothing to convert, because the result has already been
        # written directly to *rv. See the special case in outParamForm.
        "    return JS_TRUE;\n"
    }

def isVariantType(t):
    return isSpecificInterfaceType(t, 'nsIVariant')

def writeResultConv(f, type, jsvalPtr, jsvalRef):
    """ Emit code to convert the C++ variable `result` to a jsval.

    The emitted code contains a return statement; it returns JS_TRUE on
    success, JS_FALSE on error.
    """
    # From NativeData2JS.
    typeName = getBuiltinOrNativeTypeName(type)
    if typeName is not None:
        template = resultConvTemplates.get(typeName)
        if template is not None:
            values = {'jsvalRef': jsvalRef,
                      'jsvalPtr': jsvalPtr}
            f.write(substitute(template, values))
            return
        # else fall through; this type isn't supported yet
    elif isInterfaceType(type):
        if isVariantType(type):
            f.write("    return xpc_qsVariantToJsval(lccx, result, %s);\n"
                    % jsvalPtr)
            return
        else:
            f.write("    if (!result) {\n"
                    "      *%s = JSVAL_NULL;\n"
                    "      return JS_TRUE;\n"
                    "    }\n"
                    "    nsWrapperCache* cache = xpc_qsGetWrapperCache(result);\n"
                    "    if (xpc_FastGetCachedWrapper(cache, obj, %s)) {\n"
                    "      return JS_TRUE;\n"
                    "    }\n"
                    "    // After this point do not use 'result'!\n"
                    "    qsObjectHelper helper(result, cache);\n"
                    "    return xpc_qsXPCOMObjectToJsval(lccx, "
                    "helper, &NS_GET_IID(%s), &interfaces[k_%s], %s);\n"
                    % (jsvalPtr, jsvalPtr, type.name, type.name, jsvalPtr))
            return

    warn("Unable to convert result of type %s" % type.name)
    f.write("    !; // TODO - Convert `result` to jsval, store in `%s`.\n"
            % jsvalRef)
    f.write("    return xpc_qsThrow(cx, NS_ERROR_UNEXPECTED); // FIXME\n")

def anyParamRequiresCcx(member):
    for p in member.params:
        if isVariantType(p.realtype):
            return True
    return False

def memberNeedsCcx(member):
    return member.kind == 'method' and anyParamRequiresCcx(member)

def memberNeedsCallee(member):
    return memberNeedsCcx(member) or isInterfaceType(member.realtype)

def validateParam(member, param):
    def pfail(msg):
        raise UserError(
            member.iface.name + '.' + member.name + ": "
            "parameter " + param.name + ": " + msg)

    if param.iid_is is not None:
        pfail("iid_is parameters are not supported.")
    if param.size_is is not None:
        pfail("size_is parameters are not supported.")
    if param.retval:
        pfail("Unexpected retval parameter!")
    if param.paramtype in ('out', 'inout'):
        pfail("Out parameters are not supported.")
    if param.const or param.array or param.shared:
        pfail("I am a simple caveman.")

def writeQuickStub(f, customMethodCalls, member, stubName, isSetter=False):
    """ Write a single quick stub (a custom SpiderMonkey getter/setter/method)
    for the specified XPCOM interface-member. 
    """
    isAttr = (member.kind == 'attribute')
    isMethod = (member.kind == 'method')
    assert isAttr or isMethod
    isGetter = isAttr and not isSetter

    signature = "static JSBool\n"
    if isAttr:
        # JSPropertyOp signature.
        if isSetter:
            signature += "%s(JSContext *cx, JSObject *obj, jsid id, JSBool strict,%s jsval *vp)\n"
        else:
            signature += "%s(JSContext *cx, JSObject *obj, jsid id,%s jsval *vp)\n"
    else:
        # JSFastNative.
        signature += "%s(JSContext *cx, uintN argc,%s jsval *vp)\n"

    customMethodCall = customMethodCalls.get(stubName, None)

    if customMethodCall is None:
        customMethodCall = customMethodCalls.get(member.iface.name + '_', None)
        if customMethodCall is not None:
            if isMethod:
                code = customMethodCall.get('code', None)
            elif isGetter:
                code = customMethodCall.get('getter_code', None)
            else:
                code = customMethodCall.get('setter_code', None)
        else:
            code = None

        if code is not None:
            templateName = member.iface.name
            if isGetter:
                templateName += '_Get'
            elif isSetter:
                templateName += '_Set'

            # Generate the code for the stub, calling the template function
            # that's shared between the stubs. The stubs can't have additional
            # arguments, only the template function can.
            callTemplate = signature % (stubName, '')
            callTemplate += "{\n"

            nativeName = (member.binaryname is not None and member.binaryname
                          or header.firstCap(member.name))
            argumentValues = (customMethodCall['additionalArgumentValues']
                              % nativeName)
            if isAttr:
                callTemplate += ("    return %s(cx, obj, id%s, %s, vp);\n"
                                 % (templateName, ", strict" if isSetter else "", argumentValues))
            else:
                callTemplate += ("    return %s(cx, argc, %s, vp);\n"
                                 % (templateName, argumentValues))
            callTemplate += "}\n\n"

            # Fall through and create the template function stub called from the
            # real stubs, but only generate the stub once. Otherwise, just write
            # out the call to the template function and return.
            templateGenerated = templateName + '_generated'
            if templateGenerated in customMethodCall:
                f.write(callTemplate)
                return
            customMethodCall[templateGenerated] = True

            stubName = templateName
        else:
            callTemplate = ""
    else:
        callTemplate = ""
        code = customMethodCall.get('code', None)

    unwrapThisFailureFatal = (customMethodCall is None or
                              customMethodCall.get('unwrapThisFailureFatal', True));
    if (not unwrapThisFailureFatal and not isAttr):
        raise UserError(member.iface.name + '.' + member.name + ": "
                        "Unwrapping this failure must be fatal for methods")

    # Function prolog.

    # Only template functions can have additional arguments.
    if customMethodCall is None or not 'additionalArguments' in customMethodCall:
        additionalArguments = ''
    else:
        additionalArguments = " %s," % customMethodCall['additionalArguments']
    f.write(signature % (stubName, additionalArguments))
    f.write("{\n")
    f.write("    XPC_QS_ASSERT_CONTEXT_OK(cx);\n")

    # For methods, compute "this".
    if isMethod:
        f.write("    JSObject *obj = JS_THIS_OBJECT(cx, vp);\n"
                "    if (!obj)\n"
                "        return JS_FALSE;\n")

    # Create ccx if needed.
    haveCcx = memberNeedsCcx(member)
    if haveCcx and not unwrapThisFailureFatal:
        raise UserError(member.iface.name + '.' + member.name + ": "
                        "Unwrapping this failure must be fatal when we have a ccx")

    if haveCcx:
        f.write("    XPCCallContext ccx(JS_CALLER, cx, obj, "
                "JSVAL_TO_OBJECT(JS_CALLEE(cx, vp)));\n")
        if isInterfaceType(member.realtype):
            f.write("    XPCLazyCallContext lccx(ccx);\n")
    elif isInterfaceType(member.realtype):
        if isMethod:
            f.write("    JSObject *callee = "
                    "JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));\n")
        elif isGetter:
            f.write("    JSObject *callee = nsnull;\n")

    # Get the 'self' pointer.
    if customMethodCall is None or not 'thisType' in customMethodCall:
        f.write("    %s *self;\n" % member.iface.name)
    else:
        f.write("    %s *self;\n" % customMethodCall['thisType'])
    f.write("    xpc_qsSelfRef selfref;\n")
    # Don't use FromCcx for getters or setters; the way we construct the ccx in
    # a getter/setter causes it to find the wrong wrapper in some cases.
    if haveCcx:
        # Undocumented, but the interpreter puts 'this' at argv[-1],
        # which is vp[1]; and it's ok to overwrite it.
        f.write("    if (!xpc_qsUnwrapThisFromCcx(ccx, &self, &selfref.ptr, "
                "&vp[1]))\n")
        f.write("        return JS_FALSE;\n")
    else:
        if isGetter:
            pthisval = 'vp'
        elif isSetter:
            f.write("    js::AutoValueRooter tvr(cx);\n")
            pthisval = 'tvr.jsval_addr()'
        else:
            pthisval = '&vp[1]' # as above, ok to overwrite vp[1]

        if unwrapThisFailureFatal:
            unwrapFatalArg = "true"
        else:
            unwrapFatalArg = "false"

        if not isSetter and isInterfaceType(member.realtype):
            f.write("    XPCLazyCallContext lccx(JS_CALLER, cx, obj);\n")
            f.write("    if (!xpc_qsUnwrapThis(cx, obj, callee, &self, "
                    "&selfref.ptr, %s, &lccx, %s))\n" % (pthisval, unwrapFatalArg))
        else:
            f.write("    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, "
                    "&selfref.ptr, %s, nsnull, %s))\n" % (pthisval, unwrapFatalArg))
        f.write("        return JS_FALSE;\n")

        if not unwrapThisFailureFatal:
            f.write("      if (!self) {\n")
            if (isGetter):
                f.write("        *vp = JSVAL_NULL;\n")
            f.write("        return JS_TRUE;\n")
            f.write("    }\n");

    if isMethod:
        # If there are any required arguments, check argc.
        requiredArgs = len(member.params)
        while requiredArgs and member.params[requiredArgs-1].optional:
            requiredArgs -= 1
        if requiredArgs:
            f.write("    if (argc < %d)\n" % requiredArgs)
            f.write("        return xpc_qsThrow(cx, "
                    "NS_ERROR_XPC_NOT_ENOUGH_ARGS);\n")

    # Convert in-parameters.
    rvdeclared = False
    if isMethod:
        if len(member.params) > 0:
            f.write("    jsval *argv = JS_ARGV(cx, vp);\n")
        for i, param in enumerate(member.params):
            argName = 'arg%d' % i
            argTypeKey = argName + 'Type'
            if customMethodCall is None or not argTypeKey in customMethodCall:
                validateParam(member, param)
                realtype = param.realtype
            else:
                realtype = xpidl.Forward(name=customMethodCall[argTypeKey],
                                         location='', doccomments='')
            # Emit code to convert this argument from jsval.
            rvdeclared = writeArgumentUnboxing(
                f, i, argName, realtype,
                haveCcx=haveCcx,
                optional=param.optional,
                rvdeclared=rvdeclared,
                nullBehavior=param.null,
                undefinedBehavior=param.undefined)
    elif isSetter:
        rvdeclared = writeArgumentUnboxing(f, None, 'arg0', member.realtype,
                                           haveCcx=False, optional=False,
                                           rvdeclared=rvdeclared,
                                           nullBehavior=member.null,
                                           undefinedBehavior=member.undefined)

    canFail = customMethodCall is None or customMethodCall.get('canFail', True)
    if canFail and not rvdeclared:
        f.write("    nsresult rv;\n")
        rvdeclared = True

    if code is not None:
        f.write("%s\n" % code)

    if code is None or (isGetter and callTemplate is ""):
        debugGetter = code is not None
        if debugGetter:
            f.write("#ifdef DEBUG\n")
            f.write("    nsresult debug_rv;\n")
            f.write("    nsCOMPtr<%s> debug_self;\n"
                    "    CallQueryInterface(self, getter_AddRefs(debug_self));\n"
                    % member.iface.name);
            prefix = 'debug_'
        else:
            prefix = ''

        resultname = prefix + 'result'
        selfname = prefix + 'self'
        nsresultname = prefix + 'rv'

        # Prepare out-parameter.
        if isMethod or isGetter:
            writeResultDecl(f, member.realtype, resultname)

        # Call the method.
        if isMethod:
            comName = header.methodNativeName(member)
            argv = ['arg' + str(i) for i, p in enumerate(member.params)]
            if member.implicit_jscontext:
                argv.append('cx')
            if member.optional_argc:
                argv.append('NS_MIN<PRUint32>(argc, %d) - %d' % 
                            (len(member.params), requiredArgs))
            if not isVoidType(member.realtype):
                argv.append(outParamForm(resultname, member.realtype))
            args = ', '.join(argv)
        else:
            comName = header.attributeNativeName(member, isGetter)
            if isGetter:
                args = outParamForm(resultname, member.realtype)
            else:
                args = "arg0"
            if member.implicit_jscontext:
                args = "cx, " + args

        f.write("    ")
        if canFail or debugGetter:
            f.write("%s = " % nsresultname)
        f.write("%s->%s(%s);\n" % (selfname, comName, args))

        if debugGetter:
            checkSuccess = "NS_SUCCEEDED(debug_rv)"
            if canFail:
                checkSuccess += " == NS_SUCCEEDED(rv)"
            f.write("    NS_ASSERTION(%s && "
                    "xpc_qsSameResult(debug_result, result),\n"
                    "                 \"Got the wrong answer from the custom "
                    "method call!\");\n" % checkSuccess)
            f.write("#endif\n")

    if canFail:
        # Check for errors.
        f.write("    if (NS_FAILED(rv))\n")
        if isMethod:
            if haveCcx:
                f.write("        return xpc_qsThrowMethodFailedWithCcx("
                        "ccx, rv);\n")
            else:
                f.write("        return xpc_qsThrowMethodFailed("
                        "cx, rv, vp);\n")
        else:
            if isGetter:
                thisval = '*vp'
            else:
                thisval = '*tvr.jsval_addr()'
            f.write("        return xpc_qsThrowGetterSetterFailed(cx, rv, " +
                    "JSVAL_TO_OBJECT(%s), id);\n" % thisval)

    # Convert the return value.
    if isMethod or isGetter:
        writeResultConv(f, member.realtype, 'vp', '*vp')
    else:
        f.write("    return JS_TRUE;\n")

    # Epilog.
    f.write("}\n\n")

    # Now write out the call to the template function.
    if customMethodCall is not None:
        f.write(callTemplate)

# Only these types can be returned (note: no strings);
# if the type isn't one of these, then defaultTraceType is used
traceReturnTypeMap = {
    'void':             ("uint32 ", "UINT32", "0"),
    'boolean':          ("JSBool ", "BOOL", "JS_FALSE"),
    'short':            ("int32 ", "INT32", "0"),
    'unsigned short':   ("uint32 ", "UINT32", "0"),
    'long':             ("int32 ", "INT32", "0"),
    'unsigned long':    ("uint32 ", "UINT32", "0"),
    'long long':        ("jsdouble ", "DOUBLE", "0"),
    'unsigned long long': ("jsdouble ", "DOUBLE", "0"),
    'float':            ("jsdouble ", "DOUBLE", "0"),
    'double':           ("jsdouble ", "DOUBLE", "0"),
    'octet':            ("uint32 ", "UINT32", "0"),
    '[astring]':        ("JSString *", "STRING_OR_NULL", "nsnull"),
    '[domstring]':      ("JSString *", "STRING_OR_NULL", "nsnull"),
    '[cstring]':        ("JSString *", "STRING_OR_NULL", "nsnull"),
    'string':           ("JSString *", "STRING_OR_NULL", "nsnull"),
    'wstring':          ("JSString *", "STRING_OR_NULL", "nsnull")
    }

# This list extends the above list, but includes types that
# are valid for arguments only, namely strings.
traceParamTypeMap = traceReturnTypeMap.copy()
traceParamTypeMap.update({
    'void':             ("uint32 ", "UINT32"),
    'boolean':          ("JSBool ", "BOOL"),
    'short':            ("int32 ", "INT32"),
    'unsigned short':   ("uint32 ", "UINT32"),
    'long':             ("int32 ", "INT32"),
    'unsigned long':    ("uint32 ", "UINT32"),
    'long long':        ("jsdouble ", "DOUBLE"),
    'unsigned long long': ("jsdouble ", "DOUBLE"),
    'float':            ("jsdouble ", "DOUBLE"),
    'double':           ("jsdouble ", "DOUBLE"),
    'octet':            ("uint32 ", "UINT32"),
    '[astring]':        ("JSString *", "STRING"),
    '[domstring]':      ("JSString *", "STRING"),
    '[cstring]':        ("JSString *", "STRING"),
    'string':           ("JSString *", "STRING"),
    'wstring':          ("JSString *", "STRING"),
    })

defaultReturnTraceType = ("JSObject *", "OBJECT_OR_NULL", "nsnull")
defaultParamTraceType = ("js::ValueArgType ", "VALUE")

def getTraceParamType(type):
    type = getBuiltinOrNativeTypeName(type)
    assert type is not '[jsval]'
    return traceParamTypeMap.get(type, defaultParamTraceType)[0]

def getTraceReturnType(type):
    assert not isVariantType(type)
    type = getBuiltinOrNativeTypeName(type)
    assert type is not '[jsval]'
    return traceReturnTypeMap.get(type, defaultReturnTraceType)[0]

def getTraceInfoParamType(type):
    type = getBuiltinOrNativeTypeName(type)
    assert type is not '[jsval]'
    return traceParamTypeMap.get(type, defaultParamTraceType)[1]

def getTraceInfoReturnType(type):
    assert not isVariantType(type)
    type = getBuiltinOrNativeTypeName(type)
    assert type is not '[jsval]'
    return traceReturnTypeMap.get(type, defaultReturnTraceType)[1]

def getTraceInfoDefaultReturn(type):
    assert not isVariantType(type)
    type = getBuiltinOrNativeTypeName(type)
    assert type is not '[jsval]'
    return traceReturnTypeMap.get(type, defaultReturnTraceType)[2]

def getFailureString(retval, indent):
    assert indent > 0
    ret = " " * (4 * indent)
    ret += "js_SetTraceableNativeFailed(cx);\n"
    ret += " " * (4 * indent)
    ret += "return %s;\n" % retval
    ret += " " * (4 * (indent - 1))
    ret += "}\n"
    return ret
 
def writeFailure(f, retval, indent):
    f.write(getFailureString(retval, indent))

traceableArgumentConversionTemplates = {
    'octet':
          "    PRUint8 ${name} = (PRUint8) ${argVal};\n",
    'short':
          "    PRint16 ${name} = (PRint16) ${argVal};\n",
    'unsigned short':
          "    PRUint16 ${name} = (PRUint16) ${argVal};\n",
    'long':
          "    PRInt32 ${name} = (PRInt32) ${argVal};\n",
    'unsigned long':
          "    PRUint32 ${name} = (PRUint32) ${argVal};\n",
    'long long':
          "    PRInt64 ${name} = (PRInt64) ${argVal};\n",
    'unsigned long long':
          "    PRUint64 ${name} = xpc_qsDoubleToUint64(${argVal});\n",
    'boolean':
          "    bool ${name} = (bool) ${argVal};\n",
    'float':
          "    float ${name} = (float) ${argVal};\n",
    'double':
          "    jsdouble ${name} = ${argVal};\n",
    '[astring]':
          "    XPCReadableJSStringWrapper ${name};\n"
          "    if (!${name}.init(cx, ${argVal})) {\n"
          "${error}",
    '[domstring]':
          "    XPCReadableJSStringWrapper ${name};\n"
          "    if (!${name}.init(cx, ${argVal})) {\n"
          "${error}",
    '[utf8string]':
          "    size_t ${name}_length;\n"
          "    const jschar *${name}_chars = JS_GetStringCharsAndLength(cx, "
          "${argVal}, &${name}_length);\n"
          "    if (!${name}_chars) {\n"
          "${error}"
          "    NS_ConvertUTF16toUTF8 ${name}(${argVal}_chars, ${argVal}_length);\n",
    'string':
          "    size_t ${name}_length;\n"
          "    const jschar *${name}_chars = JS_GetStringCharsAndLength(cx, "
          "${argVal}, &${name}_length);\n"
          "    if (!${name}_chars) {\n"
          "${error}"
          "    NS_ConvertUTF16toUTF8 ${name}_utf8(${name}_chars, ${name}_length);\n"
          "    const char *${name} = ${name}_utf8.get();\n",
    'wstring':
          "    const jschar *${name}_chars = JS_GetStringCharsZ(cx, {argVal});\n"
          "    if (!${name}_chars) {\n"
          "${error}"
          "    const PRUnichar *${name} = ${name}_chars;\n",
    }

def writeTraceableArgumentConversion(f, member, i, name, type, haveCcx,
                                     rvdeclared):
    argVal = "_arg%d" % i

    params = {
        'name': name,
        'argVal': argVal,
        'error': getFailureString(getTraceInfoDefaultReturn(member.realtype), 2)
        }

    typeName = getBuiltinOrNativeTypeName(type)
    if typeName is not None:
        template = traceableArgumentConversionTemplates.get(typeName)
        if template is not None:
            f.write(substitute(template, params))
            return rvdeclared
        # else fall through; the type isn't supported yet.
    elif isInterfaceType(type):
        if type.name == 'nsIVariant':
            # Totally custom.
            assert haveCcx
            template = (
                "    nsCOMPtr<nsIVariant> ${name}(already_AddRefed<nsIVariant>("
                "XPCVariant::newVariant(ccx, js::ValueArgToConstRef(${argVal}))));\n"
                "    if (!${name}) {\n")
            f.write(substitute(template, params))
            writeFailure(f, getTraceInfoDefaultReturn(member.realtype), 2)
            return rvdeclared
        elif type.name == 'nsIAtom':
            # Should have special atomizing behavior.  Fall through.
            pass
        else:
            if not rvdeclared:
                f.write("    nsresult rv;\n");
            f.write("    %s *%s;\n" % (type.name, name))
            f.write("    xpc_qsSelfRef %sref;\n" % name)
            f.write("    JS::Anchor<jsval> %sanchor;\n" % name);
            f.write("    rv = xpc_qsUnwrapArg<%s>("
                    "cx, js::ValueArgToConstRef(%s), &%s, &%sref.ptr, &%sanchor.get());\n"
                    % (type.name, argVal, name, name, name))
            f.write("    if (NS_FAILED(rv)) {\n")
            if haveCcx:
                f.write("        xpc_qsThrowBadArgWithCcx(ccx, rv, %d);\n" % i)
            else:
                # XXX Fix this to return a real error!
                f.write("        xpc_qsThrowBadArgWithDetails(cx, rv, %d, "
                        "\"%s\", \"%s\");\n"
                        % (i, member.iface.name, member.name))
            writeFailure(f, getTraceInfoDefaultReturn(member.realtype), 2)
            return True

    print member
    warn("Unable to unbox argument of type %s (for traceable arg)" % type.name)
    f.write("    !; // TODO - Unbox argument %s = arg%d\n" % (name, i))
    return rvdeclared

traceableResultConvTemplates = {
    'void':
        "    return 0;\n",
    'octet':
        "    return uint32(result);\n",
    'short':
        "    return int32(result);\n",
    'unsigned short':
        "    return uint32(result);\n",
    'long':
        "    return int32(result);\n",
    'unsigned long':
        "    return uint32(result);\n",
    'long long':
        "    return jsdouble(result);\n",
    'unsigned long long':
        "    return jsdouble(result);\n",
    'boolean':
        "    return result ? JS_TRUE : JS_FALSE;\n",
    'float':
        "    return jsdouble(result);\n",
    'double':
        "    return jsdouble(result);\n",
    '[domstring]':
        "    JSString *rval;\n"
        "    if (!xpc_qsStringToJsstring(cx, result, &rval)) {\n"
        "        JS_ReportOutOfMemory(cx);\n${errorStr}"
        "    return rval;\n",
    '[astring]':
        "    JSString *rval;\n"
        "    if (!xpc_qsStringToJsstring(cx, result, &rval)) {\n"
        "        JS_ReportOutOfMemory(cx);\n${errorStr}"
        "    return rval;\n"
    }

def writeTraceableResultConv(f, type):
    typeName = getBuiltinOrNativeTypeName(type)
    assert typeName is not '[jsval]'
    if typeName is not None:
        template = traceableResultConvTemplates.get(typeName)
        if template is not None:
            values = { 'errorStr': getFailureString(
                                   getTraceInfoDefaultReturn(type), 2) }
            f.write(substitute(template, values))
            return
        # else fall through; this type isn't supported yet
    elif isInterfaceType(type):
        if isVariantType(type):
            f.write("    jsval returnVal;\n"
                    "    JSBool ok = xpc_qsVariantToJsval(lccx, result, "
                    "&returnVal);\n")
        else:
            f.write("    if (!result) {\n"
                    "      return nsnull;\n"
                    "    }\n"
                    "    nsWrapperCache* cache = xpc_qsGetWrapperCache(result);\n"
                    "    JSObject* wrapper =\n"
                    "      xpc_FastGetCachedWrapper(cache, obj);\n"
                    "    if (wrapper) {\n"
                    "      return wrapper;\n"
                    "    }\n"
                    "    // After this point do not use 'result'!\n"
                    "    qsObjectHelper helper(result, cache);\n"
                    "    jsval returnVal;\n"
                    "    JSBool ok = xpc_qsXPCOMObjectToJsval(lccx, "
                    "helper, &NS_GET_IID(%s), &interfaces[k_%s], "
                    "&returnVal);\n"
                    % (type.name, type.name))
        f.write("    if (!ok) {\n");
        writeFailure(f, getTraceInfoDefaultReturn(type), 2)
        f.write("    return JSVAL_TO_OBJECT(returnVal);\n")
        return

    warn("Unable to convert result of type %s" % typeName)
    f.write("    !; // TODO - Convert `result` to jsval, store in rval.\n")
    f.write("    return xpc_qsThrow(cx, NS_ERROR_UNEXPECTED); // FIXME\n")

def writeTraceableQuickStub(f, customMethodCalls, member, stubName):
    assert member.traceable

    traceInfo = {
        'type': getTraceInfoReturnType(member.realtype) + "_FAIL",
        'params': ["CONTEXT", "THIS"]
        }

    haveCcx = memberNeedsCcx(member)

    customMethodCall = customMethodCalls.get(stubName, None)

    if customMethodCall is None:
        customMethodCall = customMethodCalls.get(member.iface.name + '_', None)
        if customMethodCall is not None:
            # We don't support traceable templated quickstubs yet
            assert not 'code' in customMethodCall

    if customMethodCall is not None and customMethodCall.get('skipgen', False):
        return
    if (customMethodCall is not None and
        not customMethodCall.get('unwrapThisFailureFatal', True)):
        raise UserError(member.iface.name + '.' + member.name + ": "
                        "Unwrapping this failure must be fatal for traceable stubs")

    # Write the function
    f.write("static %sFASTCALL\n" % getTraceReturnType(member.realtype))
    f.write("%s(JSContext *cx, JSObject *obj" % (stubName + "_tn"))
    # This code MUST match the arguments length check in addStubMember
    if memberNeedsCallee(member):
        f.write(", JSObject *callee")
        traceInfo["params"].append("CALLEE")
    for i, param in enumerate(member.params):
        f.write(", %s_arg%d" % (getTraceParamType(param.realtype), i))
        traceInfo["params"].append(getTraceInfoParamType(param.realtype))
    f.write(")\n{\n");
    f.write("    XPC_QS_ASSERT_CONTEXT_OK(cx);\n")

    # Create ccx if needed.
    if haveCcx:
        f.write("    XPCCallContext ccx(JS_CALLER, cx, obj, callee);\n")
        if isInterfaceType(member.realtype):
            f.write("    XPCLazyCallContext lccx(ccx);\n")

    # Get the 'self' pointer.
    if customMethodCall is None or not 'thisType' in customMethodCall:
        f.write("    %s *self;\n" % member.iface.name)
    else:
        f.write("    %s *self;\n" % customMethodCall['thisType'])
    f.write("    xpc_qsSelfRef selfref;\n")
    f.write("    JS::Anchor<jsval> selfanchor;\n")
    if haveCcx:
        f.write("    if (!xpc_qsUnwrapThisFromCcx(ccx, &self, &selfref.ptr, "
                "&selfanchor.get())) {\n")
    elif (member.kind == 'method') and isInterfaceType(member.realtype):
        f.write("    XPCLazyCallContext lccx(JS_CALLER, cx, obj);\n")
        f.write("    if (!xpc_qsUnwrapThis(cx, obj, callee, &self, &selfref.ptr, "
                "&selfanchor.get(), &lccx)) {\n")
    else:
        f.write("    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, "
                "&selfanchor.get(), nsnull)) {\n")
    writeFailure(f, getTraceInfoDefaultReturn(member.realtype), 2)

    argNames = []

    # Convert in-parameters.
    rvdeclared = False
    for i, param in enumerate(member.params):
        argName = "arg%d" % i
        argTypeKey = argName + 'Type'
        if customMethodCall is None or not argTypeKey in customMethodCall:
            validateParam(member, param)
            realtype = unaliasType(param.realtype)
        else:
            realtype = xpidl.Forward(name=customMethodCall[argTypeKey],
                                     location='', doccomments='')
        rvdeclared = writeTraceableArgumentConversion(f, member, i, argName,
                                                      realtype, haveCcx,
                                                      rvdeclared)
        argNames.append(argName)

    canFail = customMethodCall is None or customMethodCall.get('canFail', True)
    if canFail and not rvdeclared:
        f.write("    nsresult rv;\n")
        rvdeclared = True

    if customMethodCall is not None and 'code' in customMethodCall:
        f.write("%s\n" % customMethodCall['code'])
    else:
        prefix = ''

        resultname = prefix + 'result'
        selfname = prefix + 'self'
        nsresultname = prefix + 'rv'

        # Prepare out-parameter.
        writeResultDecl(f, member.realtype, resultname)

        # Call the method.
        comName = header.methodNativeName(member)
        if not isVoidType(member.realtype):
            argNames.append(outParamForm(resultname, member.realtype))
        args = ', '.join(argNames)

        f.write("    ")
        if canFail:
            f.write("%s = " % nsresultname)
        f.write("%s->%s(%s);\n" % (selfname, comName, args))

    if canFail:
        # Check for errors.
        f.write("    if (NS_FAILED(rv)) {\n")
        if haveCcx:
            f.write("        xpc_qsThrowMethodFailedWithCcx(ccx, rv);\n")
        else:
            # XXX Replace with a real error message!
            f.write("        xpc_qsThrowMethodFailedWithDetails(cx, rv, "
                    "\"%s\", \"%s\");\n" % (member.iface.name, member.name))
        writeFailure(f, getTraceInfoDefaultReturn(member.realtype), 2)

    # Convert the return value.
    writeTraceableResultConv(f, member.realtype)

    # Epilog.
    f.write("}\n\n")

    # Write the JS_DEFINE_TRCINFO block
    f.write("JS_DEFINE_TRCINFO_1(%s,\n" % stubName)
    f.write("    (%d, (static, %s, %s, %s, 0, nanojit::ACCSET_STORE_ANY)))\n\n"
            % (len(traceInfo["params"]), traceInfo["type"], stubName + "_tn",
               ", ".join(traceInfo["params"])))

def writeAttrStubs(f, customMethodCalls, attr):
    cmc = customMethodCalls.get(attr.iface.name + "_" + header.methodNativeName(attr), None)
    custom = cmc and cmc.get('skipgen', False)

    getterName = (attr.iface.name + '_'
                  + header.attributeNativeName(attr, True))
    if not custom:
        writeQuickStub(f, customMethodCalls, attr, getterName)
    if attr.readonly:
        setterName = 'xpc_qsGetterOnlyPropertyStub'
    else:
        setterName = (attr.iface.name + '_'
                      + header.attributeNativeName(attr, False))
        if not custom:
            writeQuickStub(f, customMethodCalls, attr, setterName, isSetter=True)

    ps = ('{"%s", %s, %s}'
          % (attr.name, getterName, setterName))
    return ps

def writeMethodStub(f, customMethodCalls, method):
    """ Write a method stub to `f`. Return an xpc_qsFunctionSpec initializer. """

    cmc = customMethodCalls.get(method.iface.name + "_" + header.methodNativeName(method), None)
    custom = cmc and cmc.get('skipgen', False)

    stubName = method.iface.name + '_' + header.methodNativeName(method)
    if not custom:
        writeQuickStub(f, customMethodCalls, method, stubName)
    fs = '{"%s", %s, %d}' % (method.name, stubName, len(method.params))
    return fs

def writeTraceableStub(f, customMethodCalls, method):
    """ Write a method stub to `f`. Return an xpc_qsTraceableSpec initializer. """

    cmc = customMethodCalls.get(method.iface.name + "_" + header.methodNativeName(method), None)
    custom = cmc and cmc.get('skipgen', False)

    stubName = method.iface.name + '_' + header.methodNativeName(method)
    if not custom:
        writeTraceableQuickStub(f, customMethodCalls, method, stubName)
    fs = '{"%s", %s, %d}' % (method.name,
                             "JS_DATA_TO_FUNC_PTR(JSNative, &%s_trcinfo)" % stubName,
                             len(method.params))
    return fs

def writeStubsForInterface(f, customMethodCalls, iface):
    f.write("// === interface %s\n\n" % iface.name)
    propspecs = []
    funcspecs = []
    tracespecs = []
    for member in iface.stubMembers:
        if member.kind == 'attribute':
            ps = writeAttrStubs(f, customMethodCalls, member)
            propspecs.append(ps)
        elif member.kind == 'method':
            fs = writeMethodStub(f, customMethodCalls, member)
            if member.traceable:
                ts = writeTraceableStub(f, customMethodCalls, member)
                tracespecs.append(ts)
            else:
                funcspecs.append(fs)
        else:
            raise TypeError('expected attribute or method, not %r'
                            % member.__class__.__name__)

    if propspecs:
        f.write("static const xpc_qsPropertySpec %s_properties[] = {\n"
                % iface.name)
        for ps in propspecs:
            f.write("    %s,\n" % ps)
        f.write("    {nsnull}};\n")
    if funcspecs:
        f.write("static const xpc_qsFunctionSpec %s_functions[] = {\n" % iface.name)
        for fs in funcspecs:
            f.write("    %s,\n" % fs)
        f.write("    {nsnull}};\n")
    if tracespecs:
        f.write("static const xpc_qsTraceableSpec %s_traceables[] = {\n" % iface.name)
        for ts in tracespecs:
            f.write("    %s,\n" % ts)
        f.write("    {nsnull}};\n")
    f.write('\n\n')

def hashIID(iid):
    # See nsIDKey::HashCode in nsHashtable.h.
    return int(iid[:8], 16)

uuid_re = re.compile(r'^([0-9a-f]{8})-([0-9a-f]{4})-([0-9a-f]{4})-([0-9a-f]{4})-([0-9a-f]{12})$')

def writeResultXPCInterfacesArray(f, conf, resulttypes):
    f.write("// === XPCNativeInterface cache \n\n")
    count = len(resulttypes)
    if count > 0:
        f.write("static XPCNativeInterface* interfaces[%d];\n\n" % count)
    f.write("void %s_MarkInterfaces()\n"
            "{\n" % conf.name)
    if count > 0:
        f.write("    for (PRUint32 i = 0; i < %d; ++i)\n"
                "        if (interfaces[i])\n"
                "            interfaces[i]->Mark();\n" % count)
    f.write("}\n")
    f.write("void %s_ClearInterfaces()\n"
            "{\n" % conf.name)
    if count > 0:
        f.write("    memset(interfaces, 0, %d * "
                "sizeof(XPCNativeInterface*));\n" % count)
    f.write("}\n\n")
    i = 0
    for type in resulttypes:
        f.write("static const PRUint32 k_%s = %d;\n" % (type, i))
        i += 1
    if count > 0:
        f.write("\n\n")

def writeDefiner(f, conf, interfaces):
    f.write("// === Definer\n\n")

    # generate the static hash table
    loadFactor = 0.6
    size = int(len(interfaces) / loadFactor)
    buckets = [[] for i in range(size)]
    for iface in interfaces:
        # This if-statement discards interfaces specified with
        # "nsInterfaceName.*" that don't have any stub-able members.
        if iface.stubMembers:
            h = hashIID(iface.attributes.uuid)
            buckets[h % size].append(iface)

    # Calculate where each interface's entry will show up in tableData.  Where
    # there are hash collisions, the extra entries are added at the end of the
    # table.
    entryIndexes = {}
    arraySize = size
    for i, bucket in enumerate(buckets):
        if bucket:
            entryIndexes[bucket[0].attributes.uuid] = i
            for iface in bucket[1:]:
                entryIndexes[iface.attributes.uuid] = arraySize
                arraySize += 1

    entries = ["    {{0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}}, "
               "nsnull, nsnull, nsnull, XPC_QS_NULL_INDEX, XPC_QS_NULL_INDEX}"
               for i in range(arraySize)]
    for i, bucket in enumerate(buckets):
        for j, iface in enumerate(bucket):
            # iid field
            uuid = iface.attributes.uuid.lower()
            m = uuid_re.match(uuid)
            assert m is not None
            m0, m1, m2, m3, m4 = m.groups()
            m3arr = ('{0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s, 0x%s}'
                     % (m3[0:2], m3[2:4], m4[0:2], m4[2:4],
                        m4[4:6], m4[6:8], m4[8:10], m4[10:12]))
            iid = ('{0x%s, 0x%s, 0x%s, %s}' % (m0, m1, m2, m3arr))

            # properties field
            properties = "nsnull"
            for member in iface.stubMembers:
                if member.kind == 'attribute':
                    assert not member.traceable
                    properties = iface.name + "_properties"
                    break

            # member field
            functions = "nsnull"
            for member in iface.stubMembers:
                if member.kind == 'method' and not member.traceable:
                    functions = iface.name + "_functions"
                    break

            # traceables field
            traceables = "nsnull"
            for member in iface.stubMembers:
                if member.traceable:
                    traceables = iface.name + "_traceables"
                    break

            # parentInterface field
            baseName = iface.base
            while baseName is not None:
                piface = iface.idl.getName(baseName, None)
                k = entryIndexes.get(piface.attributes.uuid)
                if k is not None:
                    parentInterface = str(k)
                    break
                baseName = piface.base
            else:
                parentInterface = "XPC_QS_NULL_INDEX"

            # chain field
            if j == len(bucket) - 1:
                chain = "XPC_QS_NULL_INDEX"
            else:
                k = entryIndexes[bucket[j+1].attributes.uuid]
                chain = str(k)

            # add entry
            entry = "    {%s, %s, %s, %s, %s, %s}" % (
                iid, properties, functions, traceables, parentInterface, chain)
            entries[entryIndexes[iface.attributes.uuid]] = entry

    f.write("static const xpc_qsHashEntry tableData[] = {\n")
    f.write(",\n".join(entries))
    f.write("\n    };\n\n")

    # the definer function (entry point to this quick stubs file)
    f.write("JSBool %s_DefineQuickStubs(" % conf.name)
    f.write("JSContext *cx, JSObject *proto, uintN flags, PRUint32 count, "
            "const nsID **iids)\n"
            "{\n")
    f.write("    return xpc_qsDefineQuickStubs("
            "cx, proto, flags, count, iids, %d, tableData);\n" % size)
    f.write("}\n\n\n")


stubTopTemplate = '''\
/* THIS FILE IS AUTOGENERATED - DO NOT EDIT */
#include "jsapi.h"
#include "jscntxt.h"
/* Include nanojit.h early to avoid conflicting with nscore's |#define free|.
 * NB: needs to be kept in sync with jsbuiltins.h */
#ifdef JS_TRACER
#  include "nanojit/nanojit.h"
#endif
#include "qsWinUndefs.h"
#include "prtypes.h"
#include "nsID.h"
#include "%s"
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsDependentString.h"
#include "xpcprivate.h"  // for XPCCallContext
#include "XPCQuickStubs.h"
#include "nsWrapperCacheInlines.h"
#include "jsbuiltins.h"
'''

def writeStubFile(filename, headerFilename, conf, interfaces):
    print "Creating stub file", filename
    make_targets.append(filename)

    f = open(filename, 'w')
    filesIncluded = set()

    def includeType(type):
        type = unaliasType(type)
        if type.kind in ('builtin', 'native'):
            return None
        file = conf.irregularFilenames.get(type.name, type.name) + '.h'
        if file not in filesIncluded:
            f.write('#include "%s"\n' % file)
            filesIncluded.add(file)
        return type

    def writeIncludesForMember(member):
        assert member.kind in ('attribute', 'method')
        resulttype = includeType(member.realtype)
        if member.kind == 'method':
            for p in member.params:
                includeType(p.realtype)
        return resulttype

    def writeIncludesForInterface(iface):
        assert iface.kind == 'interface'
        resulttypes = []
        for member in iface.stubMembers:
            resulttype = writeIncludesForMember(member)
            if resulttype is not None and not isVariantType(resulttype):
                resulttypes.append(resulttype.name)
                
        includeType(iface)

        return resulttypes

    try:
        f.write(stubTopTemplate % os.path.basename(headerFilename))
        N = 256
        resulttypes = []
        for iface in interfaces:
            resulttypes.extend(writeIncludesForInterface(iface))
        resulttypes.extend(conf.customReturnInterfaces)
        for customInclude in conf.customIncludes:
            f.write('#include "%s"\n' % customInclude)
        f.write("\n\n")
        writeResultXPCInterfacesArray(f, conf, frozenset(resulttypes))
        for customQS in conf.customQuickStubs:
            f.write('#include "%s"\n' % customQS)
        for iface in interfaces:
            writeStubsForInterface(f, conf.customMethodCalls, iface)
        writeDefiner(f, conf, interfaces)
    finally:
        f.close()

def makeQuote(filename):
    return filename.replace(' ', '\\ ')  # enjoy!

def writeMakeDependOutput(filename):
    print "Creating makedepend file", filename
    f = open(filename, 'w')
    try:
        if len(make_targets) > 0:
            f.write("%s:" % makeQuote(make_targets[0]))
            for filename in make_dependencies:
                f.write(' \\\n\t\t%s' % makeQuote(filename))
            f.write('\n\n')
            for filename in make_targets[1:]:
                f.write('%s: %s\n' % (makeQuote(filename), makeQuote(make_targets[0])))
    finally:
        f.close()

def main():
    from optparse import OptionParser
    o = OptionParser(usage="usage: %prog [options] configfile")
    o.add_option('-o', "--stub-output",
                 type='string', dest='stub_output', default=None,
                 help="Quick stub C++ source output file", metavar="FILE")
    o.add_option('--header-output', type='string', default=None,
                 help="Quick stub header output file", metavar="FILE")
    o.add_option('--makedepend-output', type='string', default=None,
                 help="gnumake dependencies output file", metavar="FILE")
    o.add_option('--idlpath', type='string', default='.',
                 help="colon-separated directories to search for idl files",
                 metavar="PATH")
    o.add_option('--cachedir', dest='cachedir', default='',
                 help="Directory in which to cache lex/parse tables.")
    o.add_option("--verbose-errors", action='store_true', default=False,
                 help="When an error happens, display the Python traceback.")
    o.add_option("--enable-traceables", action='store_true', default=False,
                 help="Whether or not traceable methods are generated.")
    (options, filenames) = o.parse_args()

    if len(filenames) != 1:
        o.error("Exactly one config filename is needed.")
    filename = filenames[0]

    if options.stub_output is None:
        if filename.endswith('.qsconf') or filename.endswith('.py'):
            options.stub_output = filename.rsplit('.', 1)[0] + '.cpp'
        else:
            options.stub_output = filename + '.cpp'
    if options.header_output is None:
        options.header_output = re.sub(r'(\.c|\.cpp)?$', '.h',
                                       options.stub_output)

    if options.cachedir != '':
        sys.path.append(options.cachedir)
        if not os.path.isdir(options.cachedir):
            os.makedirs(options.cachedir)

    try:
        includePath = options.idlpath.split(':')
        conf, interfaces = readConfigFile(filename,
                                          includePath=includePath,
                                          cachedir=options.cachedir,
                                          traceable=options.enable_traceables)
        writeHeaderFile(options.header_output, conf.name)
        writeStubFile(options.stub_output, options.header_output,
                      conf, interfaces)
        if options.makedepend_output is not None:
            writeMakeDependOutput(options.makedepend_output)
    except Exception, exc:
        if options.verbose_errors:
            raise
        elif isinstance(exc, (UserError, xpidl.IDLError)):
            warn(str(exc))
        elif isinstance(exc, OSError):
            warn("%s: %s" % (exc.__class__.__name__, exc))
        else:
            raise
        sys.exit(1)

if __name__ == '__main__':
    main()
