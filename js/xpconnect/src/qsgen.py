#!/usr/bin/env/python
# qsgen.py - Generate XPConnect quick stubs.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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


import xpidl
import header
import makeutils
import os, re
import sys

# === Preliminaries

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
    makeutils.dependencies.append(filename)
    text = open(filename, 'r').read()
    idl = parser.parse(text, filename=filename)
    idl.resolve(includePath, parser)
    return idl

def removeStubMember(memberId, member):
    if member not in member.iface.stubMembers:
        raise UserError("Trying to remove member %s from interface %s, but it was never added"
                        % (member.name, member.iface.name))
    member.iface.stubMembers.remove(member)

def addStubMember(memberId, member):
    if member.kind == 'method' and not member.implicit_jscontext and not isVariantType(member.realtype):
        for param in member.params:
            for attrname, value in vars(param).items():
                if value is True:
                    if attrname == 'optional':
                        continue

                    raise UserError("Method %s, parameter %s: "
                                    "unrecognized property %r"
                                    % (memberId, param.name, attrname))

    # Add this member to the list.
    member.iface.stubMembers.append(member)

def checkStubMember(member):
    memberId = member.iface.name + "." + member.name
    if member.kind not in ('method', 'attribute'):
        raise UserError("Member %s is %r, not a method or attribute."
                        % (memberId, member.kind))
    if member.noscript:
        raise UserError("%s %s is noscript."
                        % (member.kind.capitalize(), memberId))
    if member.kind == 'method' and member.notxpcom:
        raise UserError(
            "%s %s: notxpcom methods are not supported."
            % (member.kind.capitalize(), memberId))

    if (member.kind == 'attribute'
          and not member.readonly
          and isSpecificInterfaceType(member.realtype, 'nsIVariant')):
        raise UserError(
            "Attribute %s: Non-readonly attributes of type nsIVariant "
            "are not supported."
            % memberId)

    # Check for unknown properties.
    for attrname, value in vars(member).items():
        if value is True and attrname not in ('readonly','optional_argc',
                                              'implicit_jscontext',
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
        self.customMethodCalls = config.get('customMethodCalls', {})
        self.newBindingProperties = config.get('newBindingProperties', {})

def readConfigFile(filename, includePath, cachedir):
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
            iface.newBindingProperties = 'nullptr'
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
            raise UserError("Interface %s is not scriptable." % interfaceName)

        if memberName == '*':
            if not add:
                raise UserError("Can't use negation in stub list with wildcard, in %s.*" % interfaceName)

            # Stub all scriptable members of this interface.
            for member in iface.members:
                if member.kind in ('method', 'attribute') and not member.noscript:
                    addStubMember(iface.name + '.' + member.name, member)

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

                addStubMember(memberId, member)
                if member.iface not in stubbedInterfaces:
                    stubbedInterfaces.append(member.iface)
            else:
                removeStubMember(memberId, member)

    for (interfaceName, v) in conf.newBindingProperties.iteritems():
        iface = getInterface(interfaceName, errorLoc='looking for %r' % interfaceName)
        iface.newBindingProperties = v
        if iface not in stubbedInterfaces:
            stubbedInterfaces.append(iface)

    # Now go through and check all the interfaces' members
    for iface in stubbedInterfaces:
        for member in iface.stubMembers:
            checkStubMember(member)

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
                "bool " + name + "_DefineQuickStubs("
                "JSContext *cx, JSObject *proto, unsigned flags, "
                "uint32_t count, const nsID **iids);\n\n"
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

class StringTable:
    def __init__(self):
        self.current_index = 0;
        self.table = {}
        self.reverse_table = {}

    def c_strlen(self, string):
        return len(string) + 1

    def stringIndex(self, string):
        if string in self.table:
            return self.table[string]
        else:
            result = self.current_index
            self.table[string] = result
            self.current_index += self.c_strlen(string)
            return result

    def writeDefinition(self, f, name):
        entries = self.table.items()
        entries.sort(key=lambda x:x[1])
        # Avoid null-in-string warnings with GCC and potentially
        # overlong string constants; write everything out the long way.
        def explodeToCharArray(string):
            return ", ".join(map(lambda x:"'%s'" % x, string))
        f.write("static const char %s[] = {\n" % name)
        for (string, offset) in entries[:-1]:
            f.write("  /* %5d */ %s, '\\0',\n"
                    % (offset, explodeToCharArray(string)))
        f.write("  /* %5d */ %s, '\\0' };\n\n"
                % (entries[-1][1], explodeToCharArray(entries[-1][0])))
        f.write("const char* xpc_qsStringTable = %s;\n\n" % name);

def substitute(template, vals):
    """ Simple replacement for string.Template, which isn't in Python 2.3. """
    def replacement(match):
        return vals[match.group(1)]
    return re.sub(r'\${(\w+)}', replacement, template)

# From JSData2Native.
argumentUnboxingTemplates = {
    'octet':
        "    uint32_t ${name}_u32;\n"
        "    if (!JS_ValueToECMAUint32(cx, ${argVal}, &${name}_u32))\n"
        "        return false;\n"
        "    uint8_t ${name} = (uint8_t) ${name}_u32;\n",

    'short':
        "    int32_t ${name}_i32;\n"
        "    if (!JS::ToInt32(cx, ${argVal}, &${name}_i32))\n"
        "        return false;\n"
        "    int16_t ${name} = (int16_t) ${name}_i32;\n",

    'unsigned short':
        "    uint32_t ${name}_u32;\n"
        "    if (!JS_ValueToECMAUint32(cx, ${argVal}, &${name}_u32))\n"
        "        return false;\n"
        "    uint16_t ${name} = (uint16_t) ${name}_u32;\n",

    'long':
        "    int32_t ${name};\n"
        "    if (!JS::ToInt32(cx, ${argVal}, &${name}))\n"
        "        return false;\n",

    'unsigned long':
        "    uint32_t ${name};\n"
        "    if (!JS_ValueToECMAUint32(cx, ${argVal}, &${name}))\n"
        "        return false;\n",

    'long long':
        "    int64_t ${name};\n"
        "    if (!JS::ToInt64(cx, ${argVal}, &${name}))\n"
        "        return false;\n",

    'unsigned long long':
        "    uint64_t ${name};\n"
        "    if (!JS::ToUint64(cx, ${argVal}, &${name}))\n"
        "        return false;\n",

    'float':
        "    double ${name}_dbl;\n"
        "    if (!JS_ValueToNumber(cx, ${argVal}, &${name}_dbl))\n"
        "        return false;\n"
        "    float ${name} = (float) ${name}_dbl;\n",

    'double':
        "    double ${name};\n"
        "    if (!JS_ValueToNumber(cx, ${argVal}, &${name}))\n"
        "        return false;\n",

    'boolean':
        "    bool ${name};\n"
        "    JS_ValueToBoolean(cx, ${argVal}, &${name});\n",

    '[astring]':
        "    xpc_qsAString ${name}(cx, ${argVal}, ${argPtr});\n"
        "    if (!${name}.IsValid())\n"
        "        return false;\n",

    '[domstring]':
        "    xpc_qsDOMString ${name}(cx, ${argVal}, ${argPtr},\n"
        "                            xpc_qsDOMString::e${nullBehavior},\n"
        "                            xpc_qsDOMString::e${undefinedBehavior});\n"
        "    if (!${name}.IsValid())\n"
        "        return false;\n",

    'string':
        "    JSAutoByteString ${name}_bytes;\n"
        "    if (!xpc_qsJsvalToCharStr(cx, ${argVal}, &${name}_bytes))\n"
        "        return false;\n"
        "    char *${name} = ${name}_bytes.ptr();\n",

    'wstring':
        "    const PRUnichar *${name};\n"
        "    if (!xpc_qsJsvalToWcharStr(cx, ${argVal}, ${argPtr}, &${name}))\n"
        "        return false;\n",

    '[cstring]':
        "    xpc_qsACString ${name}(cx, ${argVal}, ${argPtr});\n"
        "    if (!${name}.IsValid())\n"
        "        return false;\n",

    '[utf8string]':
        "    xpc_qsAUTF8String ${name}(cx, ${argVal}, ${argPtr});\n"
        "    if (!${name}.IsValid())\n"
        "        return false;\n",

    '[jsval]':
        "    JS::RootedValue ${name}(cx, ${argVal});\n"
    }

# From JSData2Native.
#
# Omitted optional arguments are treated as though the caller had passed JS
# `null`; this behavior is from XPCWrappedNative::CallMethod. The 'jsval' type,
# however, defaults to 'undefined'.
#
def writeArgumentUnboxing(f, i, name, type, optional, rvdeclared,
                          nullBehavior, undefinedBehavior,
                          propIndex=None):
    # f - file to write to
    # i - int or None - Indicates the source jsval.  If i is an int, the source
    #     jsval is argv[i]; otherwise it is argv[0].  But if Python i >= C++ argc,
    #     which can only happen if optional is True, the argument is missing;
    #     use JSVAL_NULL as the source jsval instead.
    # name - str - name of the native C++ variable to create.
    # type - xpidl.{Interface,Native,Builtin} - IDL type of argument
    # optional - bool - True if the parameter is optional.
    # rvdeclared - bool - False if no |nsresult rv| has been declared earlier.

    typeName = getBuiltinOrNativeTypeName(type)

    isSetter = (i is None)

    if isSetter:
        argPtr = "argv[0].address()"
        argVal = "argv[0]"
    elif optional:
        if typeName == "[jsval]":
            val = "JS::UndefinedHandleValue"
        else:
            val = "JS::NullHandleValue"
        argVal = "(%d < argc ? argv[%d] : %s)" % (i, i, val)
        if typeName == "[jsval]":
            # This should use the rooted argument,
            # however we probably won't ever need to support that.
            argPtr = None
        else:
            argPtr = "(%d < argc ? argv[%d].address() : NULL)" % (i, i)
    else:
        argVal = "argv[%d]" % i
        argPtr = argVal + ".address()"

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
            template = (
                "    nsCOMPtr<nsIVariant> ${name}(already_AddRefed<nsIVariant>("
                "XPCVariant::newVariant(cx, ${argVal})));\n"
                "    if (!${name}) {\n"
                "        xpc_qsThrowBadArg(cx, NS_ERROR_INVALID_ARG, vp, %d);\n"
                "        return false;\n"
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
                assert(propIndex is not None)
                f.write("        xpc_qsThrowBadSetterValue(cx, rv, JSVAL_TO_OBJECT(vp[1]), (uint16_t)%s);\n" %
                        propIndex)
            else:
                f.write("        xpc_qsThrowBadArg(cx, rv, vp, %d);\n" % i)
            f.write("        return false;\n"
                    "    }\n")
            return True

    warn("Unable to unbox argument of type %s (native type %s)" % (type.name, typeName))
    if i is None:
        src = 'argv[0]'
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
            f.write("    JS::RootedValue %s(cx);\n" % varname)
            return
    elif t.kind in ('interface', 'forward'):
        f.write("    nsCOMPtr<%s> %s;\n" % (type.name, varname))
        return

    warn("Unable to declare result of type %s" % type.name)
    f.write("    !; // TODO - Declare out parameter `%s`.\n" % varname)

def outParamForm(name, type):
    type = unaliasType(type)
    if type.kind == 'builtin':
        return '&' + name
    elif type.kind == 'native':
        if getBuiltinOrNativeTypeName(type) == '[jsval]':
            return name + '.address()'
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
            "    return true;\n",

    'octet':
        "    ${jsvalRef} = INT_TO_JSVAL((int32_t) result);\n"
        "    return true;\n",

    'short':
        "    ${jsvalRef} = INT_TO_JSVAL((int32_t) result);\n"
        "    return true;\n",

    'long':
        "    ${jsvalRef} = INT_TO_JSVAL(result);\n"
        "    return true;\n",

    'long long':
        "    return xpc_qsInt64ToJsval(cx, result, ${jsvalPtr});\n",

    'unsigned short':
        "    ${jsvalRef} = INT_TO_JSVAL((int32_t) result);\n"
        "    return true;\n",

    'unsigned long':
        "    ${jsvalRef} = UINT_TO_JSVAL(result);\n"
        "    return true;\n",

    'unsigned long long':
        "    return xpc_qsUint64ToJsval(cx, result, ${jsvalPtr});\n",

    'float':
        "    ${jsvalRef} = JS_NumberValue(result);\n"
        "    return true;\n",

    'double':
        "    ${jsvalRef} =  JS_NumberValue(result);\n"
        "    return true;\n",

    'boolean':
        "    ${jsvalRef} = (result ? JSVAL_TRUE : JSVAL_FALSE);\n"
        "    return true;\n",

    '[astring]':
        "    return xpc::StringToJsval(cx, result, ${jsvalPtr});\n",

    '[domstring]':
        "    return xpc::StringToJsval(cx, result, ${jsvalPtr});\n",

    '[jsval]':
        "    ${jsvalRef} = result;\n"
        "    return JS_WrapValue(cx, ${jsvalPtr});\n"
    }

def isVariantType(t):
    return isSpecificInterfaceType(t, 'nsIVariant')

def writeResultConv(f, type, jsvalPtr, jsvalRef):
    """ Emit code to convert the C++ variable `result` to a jsval.

    The emitted code contains a return statement; it returns true on
    success, false on error.
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
            f.write("    return xpc_qsVariantToJsval(cx, result, %s);\n"
                    % jsvalPtr)
            return
        else:
            f.write("    if (!result) {\n"
                    "      *%s = JSVAL_NULL;\n"
                    "      return true;\n"
                    "    }\n"
                    "    nsWrapperCache* cache = xpc_qsGetWrapperCache(result);\n"
                    "    if (xpc_FastGetCachedWrapper(cache, obj, %s)) {\n"
                    "      return true;\n"
                    "    }\n"
                    "    // After this point do not use 'result'!\n"
                    "    qsObjectHelper helper(result, cache);\n"
                    "    return xpc_qsXPCOMObjectToJsval(cx, "
                    "helper, &NS_GET_IID(%s), &interfaces[k_%s], %s);\n"
                    % (jsvalPtr, jsvalPtr, type.name, type.name, jsvalPtr))
            return

    warn("Unable to convert result of type %s" % type.name)
    f.write("    !; // TODO - Convert `result` to jsval, store in `%s`.\n"
            % jsvalRef)
    f.write("    return xpc_qsThrow(cx, NS_ERROR_UNEXPECTED); // FIXME\n")

def memberNeedsCallee(member):
    return isInterfaceType(member.realtype)

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

def setOptimizationForMSVC(f, b):
    """ Write a pragma that turns optimizations off (if b is False) or
    on (if b is True) for MSVC.
    """
    if b:
        pragmaParam = "on"
    else:
        pragmaParam = "off"
    f.write("#ifdef _MSC_VER\n")
    f.write('# pragma optimize("", %s)\n'%pragmaParam)
    f.write("#endif\n")

def writeQuickStub(f, customMethodCalls, stringtable, member, stubName,
                   isSetter=False):
    """ Write a single quick stub (a custom SpiderMonkey getter/setter/method)
    for the specified XPCOM interface-member. 
    """
    # Workaround for suspected compiler bug.
    # See https://bugzilla.mozilla.org/show_bug.cgi?id=750019
    disableOptimizationForMSVC = (stubName == 'nsIDOMHTMLDocument_Write')

    isAttr = (member.kind == 'attribute')
    isMethod = (member.kind == 'method')
    assert isAttr or isMethod
    isGetter = isAttr and not isSetter

    signature = ("static bool\n" +
                 "%s(JSContext *cx, unsigned argc,%s jsval *vp)\n")

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
    if disableOptimizationForMSVC:
        setOptimizationForMSVC(f, False)
    f.write(signature % (stubName, additionalArguments))
    f.write("{\n")
    f.write("    XPC_QS_ASSERT_CONTEXT_OK(cx);\n")

    # Compute "this".
    f.write("    JS::RootedObject obj(cx, JS_THIS_OBJECT(cx, vp));\n"
            "    if (!obj)\n"
            "        return false;\n")

    # Get the 'self' pointer.
    if customMethodCall is None or not 'thisType' in customMethodCall:
        f.write("    %s *self;\n" % member.iface.name)
    else:
        f.write("    %s *self;\n" % customMethodCall['thisType'])
    f.write("    xpc_qsSelfRef selfref;\n")
    pthisval = '&vp[1]' # as above, ok to overwrite vp[1]

    if unwrapThisFailureFatal:
        unwrapFatalArg = "true"
    else:
        unwrapFatalArg = "false"

    f.write("    if (!xpc_qsUnwrapThis(cx, obj, &self, "
            "&selfref.ptr, %s, %s))\n" % (pthisval, unwrapFatalArg))
    f.write("        return false;\n")

    if not unwrapThisFailureFatal:
        f.write("      if (!self) {\n")
        if (isGetter):
            f.write("        *vp = JSVAL_NULL;\n")
        f.write("        return true;\n")
        f.write("    }\n");

    if isMethod:
        # If there are any required arguments, check argc.
        requiredArgs = len(member.params)
        while requiredArgs and member.params[requiredArgs-1].optional:
            requiredArgs -= 1
    elif isSetter:
        requiredArgs = 1
    else:
        requiredArgs = 0
    if requiredArgs:
        f.write("    if (argc < %d)\n" % requiredArgs)
        f.write("        return xpc_qsThrow(cx, "
                "NS_ERROR_XPC_NOT_ENOUGH_ARGS);\n")

    # Convert in-parameters.
    rvdeclared = False
    if isMethod:
        if len(member.params) > 0:
            f.write("    JS::CallArgs argv = JS::CallArgsFromVp(argc, vp);\n")
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
                optional=param.optional,
                rvdeclared=rvdeclared,
                nullBehavior=param.null,
                undefinedBehavior=param.undefined)
    elif isSetter:
        f.write("    JS::CallArgs argv = JS::CallArgsFromVp(argc, vp);\n")
        rvdeclared = writeArgumentUnboxing(f, None, 'arg0', member.realtype,
                                           optional=False,
                                           rvdeclared=rvdeclared,
                                           nullBehavior=member.null,
                                           undefinedBehavior=member.undefined,
                                           propIndex=stringtable.stringIndex(member.name))

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
                argv.append('std::min<uint32_t>(argc, %d) - %d' %
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
            f.write("    MOZ_ASSERT(%s && "
                    "xpc_qsSameResult(debug_result, result),\n"
                    "               \"Got the wrong answer from the custom "
                    "method call!\");\n" % checkSuccess)
            f.write("#endif\n")

    if canFail:
        # Check for errors.
        f.write("    if (NS_FAILED(rv))\n")
        if isMethod:
            f.write("        return xpc_qsThrowMethodFailed("
                    "cx, rv, vp);\n")
        else:
            f.write("        return xpc_qsThrowGetterSetterFailed(cx, rv, " +
                    "JSVAL_TO_OBJECT(vp[1]), (uint16_t)%d);\n" %
                    stringtable.stringIndex(member.name))

    # Convert the return value.
    if isMethod or isGetter:
        writeResultConv(f, member.realtype, 'vp', '*vp')
    else:
        f.write("    return true;\n")

    # Epilog.
    f.write("}\n")
    if disableOptimizationForMSVC:
        setOptimizationForMSVC(f, True)
    f.write("\n")

    # Now write out the call to the template function.
    if customMethodCall is not None:
        f.write(callTemplate)

def writeAttrStubs(f, customMethodCalls, stringtable, attr):
    getterName = (attr.iface.name + '_'
                  + header.attributeNativeName(attr, True))
    writeQuickStub(f, customMethodCalls, stringtable, attr, getterName)
    if attr.readonly:
        setterName = 'xpc_qsGetterOnlyNativeStub'
    else:
        setterName = (attr.iface.name + '_'
                      + header.attributeNativeName(attr, False))
        writeQuickStub(f, customMethodCalls, stringtable, attr, setterName,
                       isSetter=True)

    ps = ('{%d, %s, %s}'
          % (stringtable.stringIndex(attr.name), getterName, setterName))
    return ps

def writeMethodStub(f, customMethodCalls, stringtable, method):
    """ Write a method stub to `f`. Return an xpc_qsFunctionSpec initializer. """

    stubName = method.iface.name + '_' + header.methodNativeName(method)
    writeQuickStub(f, customMethodCalls, stringtable, method, stubName)
    fs = '{%d, %d, %s}' % (stringtable.stringIndex(method.name),
                           len(method.params), stubName)
    return fs

def writeStubsForInterface(f, customMethodCalls, stringtable, iface):
    f.write("// === interface %s\n\n" % iface.name)
    propspecs = []
    funcspecs = []
    for member in iface.stubMembers:
        if member.kind == 'attribute':
            ps = writeAttrStubs(f, customMethodCalls, stringtable, member)
            propspecs.append(ps)
        elif member.kind == 'method':
            fs = writeMethodStub(f, customMethodCalls, stringtable, member)
            funcspecs.append(fs)
        else:
            raise TypeError('expected attribute or method, not %r'
                            % member.__class__.__name__)

    iface.propspecs = propspecs
    iface.funcspecs = funcspecs

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
        f.write("    for (uint32_t i = 0; i < %d; ++i)\n"
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
        f.write("static const uint32_t k_%s = %d;\n" % (type, i))
        i += 1
    if count > 0:
        f.write("\n\n")

def writeSpecs(f, elementType, varname, spec_type, spec_indices, interfaces):
    index = 0
    f.write("static const %s %s[] = {\n" % (elementType, varname))
    for iface in interfaces:
        specs = getattr(iface, spec_type)
        if specs:
            spec_indices[iface.name] = index
            f.write("    // %s (index %d)\n" % (iface.name,index))
            for s in specs:
                f.write("    %s,\n" % s)
            index += len(specs)
    f.write("};\n\n")

def writeDefiner(f, conf, stringtable, interfaces):
    f.write("// === Definer\n\n")

    # Write out the properties and functions
    propspecs_indices = {}
    funcspecs_indices = {}
    prop_array_name = "all_properties"
    func_array_name = "all_functions"
    writeSpecs(f, "xpc_qsPropertySpec", prop_array_name,
               "propspecs", propspecs_indices, interfaces)
    writeSpecs(f, "xpc_qsFunctionSpec", func_array_name,
               "funcspecs", funcspecs_indices, interfaces)

    # generate the static hash table
    loadFactor = 0.6
    size = int(len(interfaces) / loadFactor)
    buckets = [[] for i in range(size)]
    for iface in interfaces:
        # This if-statement discards interfaces specified with
        # "nsInterfaceName.*" that don't have any stub-able members.
        if iface.stubMembers or iface.newBindingProperties:
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
               "0, 0, 0, 0, nullptr, XPC_QS_NULL_INDEX, XPC_QS_NULL_INDEX}"
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

            # properties fields
            prop_index = 0
            prop_n_entries = 0
            if iface.propspecs:
                prop_index = propspecs_indices[iface.name]
                prop_n_entries = len(iface.propspecs)

            # member fields
            func_index = 0
            func_n_entries = 0
            if iface.funcspecs:
                func_index = funcspecs_indices[iface.name]
                func_n_entries = len(iface.funcspecs)

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
            entry = "    /* %s */ {%s, %d, %d, %d, %d, %s, %s, %s}" % (
                iface.name, iid, prop_index, prop_n_entries,
                func_index, func_n_entries, iface.newBindingProperties,
                parentInterface, chain)
            entries[entryIndexes[iface.attributes.uuid]] = entry

    f.write("static const xpc_qsHashEntry tableData[] = {\n")
    f.write(",\n".join(entries))
    f.write("\n    };\n\n")
    f.write("// Make sure our table indices aren't overflowed\n"
            "PR_STATIC_ASSERT((sizeof(tableData) / sizeof(tableData[0])) < (1 << (8 * sizeof(tableData[0].parentInterface))));\n"
            "PR_STATIC_ASSERT((sizeof(tableData) / sizeof(tableData[0])) < (1 << (8 * sizeof(tableData[0].chain))));\n\n")

    # The string table for property and method names.
    table_name = "stringtab"
    stringtable.writeDefinition(f, table_name)
    structNames = [prop_array_name, func_array_name]
    for name in structNames:
        f.write("PR_STATIC_ASSERT(sizeof(%s) < (1 << (8 * sizeof(%s[0].name_index))));\n"
                % (table_name, name))
    f.write("\n")

    # the definer function (entry point to this quick stubs file)
    f.write("namespace xpc {\n")
    f.write("bool %s_DefineQuickStubs(" % conf.name)
    f.write("JSContext *cx, JSObject *proto, unsigned flags, uint32_t count, "
            "const nsID **iids)\n"
            "{\n")
    f.write("    return !!xpc_qsDefineQuickStubs("
            "cx, proto, flags, count, iids, %d, tableData, %s, %s, %s);\n" % (
            size, prop_array_name, func_array_name, table_name))
    f.write("}\n")
    f.write("} // namespace xpc\n\n\n")


stubTopTemplate = '''\
/* THIS FILE IS AUTOGENERATED - DO NOT EDIT */
#include "jsapi.h"
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
#include <algorithm>
'''

def writeStubFile(filename, headerFilename, conf, interfaces):
    print "Creating stub file", filename
    makeutils.targets.append(filename)

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
        resulttypes = []
        for iface in interfaces:
            resulttypes.extend(writeIncludesForInterface(iface))
        for customInclude in conf.customIncludes:
            f.write('#include "%s"\n' % customInclude)
        f.write("\n\n")
        writeResultXPCInterfacesArray(f, conf, frozenset(resulttypes))
        stringtable = StringTable()
        for iface in interfaces:
            writeStubsForInterface(f, conf.customMethodCalls, stringtable, iface)
        writeDefiner(f, conf, stringtable, interfaces)
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
                                          cachedir=options.cachedir)
        writeStubFile(options.stub_output, options.header_output,
                      conf, interfaces)
        writeHeaderFile(options.header_output, conf.name)
        if options.makedepend_output is not None:
            makeutils.writeMakeDependOutput(options.makedepend_output)
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
