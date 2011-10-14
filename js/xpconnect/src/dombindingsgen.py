#!/usr/bin/env/python
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
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

from codegen import *
import xpidl
import os, re
import string
import UserDict

# === Preliminaries

# --makedepend-output support.
make_dependencies = []
make_targets = []

# === Reading the file

def addStubMember(memberId, member):
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
    if member.notxpcom:
        raise UserError(
            "%s %s: notxpcom methods are not supported."
            % (member.kind.capitalize(), memberId))

    # Check for unknown properties.
    for attrname, value in vars(member).items():
        if value is True and attrname not in ('readonly','optional_argc',
                                              'implicit_jscontext','getter',
                                              'stringifier'):
            raise UserError("%s %s: unrecognized property %r"
                            % (member.kind.capitalize(), memberId,
                               attrname))

def loadIDL(parser, includePath, filename):
    make_dependencies.append(filename)
    text = open(filename, 'r').read()
    idl = parser.parse(text, filename=filename)
    idl.resolve(includePath, parser)
    return idl


def firstCap(str):
    return str[0].upper() + str[1:]

class DOMClass(UserDict.DictMixin):
    def __init__(self, name, nativeClass, prefable):
        self.name = name
        self.base = None
        self.isBase = False
        self.nativeClass = nativeClass
        self.indexGetter = None
        self.indexSetter = None
        self.nameGetter = None
        self.nameSetter = None
        self.stringifier = False
        self.members = set()
        self.prefable = prefable

    @staticmethod
    def getterNativeType(getter):
        if isStringType(getter.realtype):
            return 'nsString'
        type = getter.realtype
        if type.kind in ('interface', 'forward'):
            if not getter.notxpcom:
                return "nsCOMPtr<%s>" % type.name
            if len(getter.params) > 1:
                assert len(getter.params) == 2
                assert getter.params[1].realtype.kind == 'native' and getter.params[1].realtype.nativename == 'nsWrapperCache'
                return 'nsISupportsResult' 
        return type.nativeType('in').strip()

    @staticmethod
    def getterNativeCall(getter):
        if isStringType(getter.realtype):
            template = ("    list->%s(index, item);\n"
                        "    return !DOMStringIsNull(item);\n")
        else:
            type = getter.realtype
            if type.kind in ('interface', 'forward'):
                if not getter.notxpcom:
                    template = "    return NS_SUCCEEDED(list->%s(index, getter_AddRefs(item)));\n"
                elif len(getter.params) > 1:
                    template = ("    item.mResult = list->%s(index, &item.mCache);\n"
                                "    return !!item.mResult;\n")
                else:
                    template = ("    item = list->%s(index);\n"
                                "    return !!item;\n")
            else:
                template = ("    item = list->%s(index);\n"
                            "    return !!item;\n")

        return template % header.methodNativeName(getter)

    @staticmethod
    def setterNativeCall(setter):
        if setter.notxpcom:
            template = ("    !; // TODO")
        else:
            template = ("    nsresult rv = list->%s(index, item);\n"
                        "    return NS_SUCCEEDED(rv) ? true : Throw(cx, rv);\n")

        return template % header.methodNativeName(setter)

    def __setattr__(self, name, value):
        self.__dict__[name] = value
        if value:
            if name == 'indexGetter':
                if value.forward:
                    self.realIndexGetter = value.iface.namemap[value.forward]
                else:
                    self.realIndexGetter = value
                self.indexGetterType = self.getterNativeType(self.realIndexGetter)
            elif name == 'indexSetter':
                if value.forward:
                    self.realIndexSetter = value.iface.namemap[value.forward]
                else:
                    self.realIndexSetter = value
                self.indexSetterType = self.realIndexSetter.params[1].realtype.nativeType("in")
            elif name == 'nameGetter':
                if value.forward:
                    self.realNameGetter = value.iface.namemap[value.forward]
                else:
                    self.realNameGetter = value
                self.nameGetterType = self.getterNativeType(self.realNameGetter)
            elif name == 'nameSetter':
                if value.forward:
                    self.realNameSetter = value.iface.namemap[value.forward]
                else:
                    self.realNameSetter = value
                self.nameSetterType = self.getterNativeType(self.realNameSetter)

    def __getitem__(self, key):
        assert type(key) == str

        if key == 'indexGet':
            return DOMClass.getterNativeCall(self.realIndexGetter)

        if key == 'indexSet':
            return DOMClass.setterNativeCall(self.realIndexSetter)

        if key == 'nameGet':
            return DOMClass.getterNativeCall(self.realNameGetter)

        if key == 'nameSet':
            return DOMClass.setterNativeCall(self.realNameSetter)

        def ops(getterType, setterType):
            def opType(type):
                return type + (" " if type.endswith('>') else "")

            if getterType or setterType:
                opsClass = ", Ops<"
                if getterType:
                    opsClass += "Getter<" + opType(getterType) + ">"
                else:
                    # Should we even support this?
                    opsClass += "NoOp"
                if setterType:
                    opsClass += ", Setter<" + opType(setterType) + ">"
                opsClass += " >"
            else:
                opsClass = ", NoOps"
            return opsClass

        if key == 'indexOps':
            return ops(self.indexGetter and self.indexGetterType, self.indexSetter and self.indexSetterType)
        if key == 'nameOps':
            return ops(self.nameGetter and self.nameGetterType, self.nameSetter and self.nameSetterType) if self.nameGetter else ""

        if key == 'listClass':
            if self.base:
                template = "DerivedListClass<${nativeClass}, ${base}Wrapper${indexOps}${nameOps} >"
            else:
                template = "ListClass<${nativeClass}${indexOps}${nameOps} >"
            return string.Template(template).substitute(self)

        return self.__dict__[key]

    def __cmp__(x, y):
        if x.isBase != y.isBase:
            return -1 if x.isBase else 1
        return cmp(x.name, y.name)

class Configuration:
    def __init__(self, filename, includePath):
        self.includePath = includePath
        config = {}
        execfile(filename, config)

        # required settings
        if 'classes' not in config:
            raise UserError(filename + ": `%s` was not defined." % name)
        self.classes = {}
        for clazz in config['classes']:
            self.classes[clazz] = DOMClass(name=clazz, nativeClass=config['classes'][clazz], prefable=False)
        if 'prefableClasses' in config:
            for clazz in config['prefableClasses']:
                self.classes[clazz] = DOMClass(name=clazz, nativeClass=config['prefableClasses'][clazz], prefable=True)
        # optional settings
        self.customInheritance = config.get('customInheritance', {})
        self.derivedClasses = {}
        self.irregularFilenames = config.get('irregularFilenames', {})
        self.customIncludes = config.get('customIncludes', [])

def readConfigFile(filename, includePath):
    # Read the config file.
    return Configuration(filename, includePath)

def completeConfiguration(conf, includePath, cachedir):
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
            if not iface.attributes.scriptable:
                raise UserError("Interface %s is not scriptable. "
                                "IDL file: %r." % (interfaceName, idlFile))
            iface.stubMembers = []
            interfaces.append(iface)
            interfacesByName[interfaceName] = iface
        return iface

    stubbedInterfaces = []

    for clazz in conf.classes.itervalues():
        interfaceName = 'nsIDOM' + clazz.name

        iface = getInterface(interfaceName, errorLoc='looking for %r' % clazz.name)

        for member in iface.members:
            if member.kind in ('method', 'attribute') and not member.noscript:
                #addStubMember(iface.name + '.' + member.name, member)
                clazz.members.add(member)

        # Stub all scriptable members of this interface.
        while True:
            if iface not in stubbedInterfaces:
                stubbedInterfaces.append(iface)
            if not clazz.indexGetter and iface.ops['index']['getter']:
                clazz.indexGetter = iface.ops['index']['getter']
            if not clazz.indexSetter and iface.ops['index']['setter']:
                clazz.indexSetter = iface.ops['index']['setter']
            if not clazz.nameGetter and iface.ops['name']['getter']:
                clazz.nameGetter = iface.ops['name']['getter']
            if not clazz.nameSetter and iface.ops['name']['setter']:
                clazz.nameSetter = iface.ops['name']['setter']
            if not clazz.stringifier and iface.ops['stringifier']:
                clazz.stringifier = iface.ops['stringifier']
            interfaceName = conf.customInheritance.get(iface.name, iface.base)
            iface = getInterface(interfaceName, errorLoc='looking for %r' % clazz.name)
            if iface.name == 'nsISupports':
                break

            assert iface.name.startswith('nsIDOM') and not iface.name.startswith('nsIDOMNS')
            clazz.base = iface.name[6:]
            # For now we only support base classes that are real DOM classes
            assert clazz.base in conf.classes
            if not conf.classes[clazz.base].isBase:
                conf.classes[clazz.base].isBase = True
                conf.derivedClasses[clazz.base] = []
            conf.derivedClasses[clazz.base].append(clazz.name)

    # Now go through and check all the interfaces' members
    for iface in stubbedInterfaces:
        for member in iface.stubMembers:
            checkStubMember(member)

    return interfaces

# === Generating the header file

def addType(types, type, map):
    def getTranslatedType(type):
        return map.get(type, type)

    type = xpidl.unaliasType(type)
    if isInterfaceType(type) or (type.kind == 'native' and type.specialtype is None):
        types.add(getTranslatedType(type.name))


def getTypes(classes, map):
    types = set()
    for clazz in classes.itervalues():
        types.add(map.get(clazz.nativeClass, clazz.nativeClass))
        if clazz.indexGetter:
            addType(types, clazz.realIndexGetter.realtype, map)
        if clazz.indexSetter:
            addType(types, clazz.realIndexSetter.realtype, map)
        if clazz.nameGetter:
            addType(types, clazz.realNameGetter.realtype, map)
        if clazz.nameSetter:
            addType(types, clazz.realNameSetter.realtype, map)
    return types

listDefinitionTemplate = (
"class ${name} {\n"
"public:\n"
"    template<typename I>\n"
"    static JSObject *create(JSContext *cx, XPCWrappedNativeScope *scope, I *list, bool *triedToWrap)\n"
"    {\n"
"        return create(cx, scope, list, GetWrapperCache(list), triedToWrap);\n"
"    }\n"
"\n"
"    static bool objIsWrapper(JSObject *obj);\n"
"    static ${nativeClass} *getNative(JSObject *obj);\n"
"\n"
"private:\n"
"    static JSObject *create(JSContext *cx, XPCWrappedNativeScope *scope, ${nativeClass} *list, nsWrapperCache *cache, bool *triedToWrap);\n"
"};"
"\n"
"\n")

def writeHeaderFile(filename, config):
    print "Creating header file", filename

    headerMacro = '__gen_%s__' % filename.replace('.', '_')
    f = open(filename, 'w')
    try:
        f.write("/* THIS FILE IS AUTOGENERATED - DO NOT EDIT */\n\n"
                "#ifndef " + headerMacro + "\n"
                "#define " + headerMacro + "\n\n")

        namespaces = []
        for type in sorted(getTypes(config.classes, {})):
            newNamespaces = type.split('::')
            type = newNamespaces.pop()
            j = 0
            for i in range(min(len(namespaces), len(newNamespaces))):
                if namespaces[i] != newNamespaces[i]:
                    break
                j += 1
            for i in range(j, len(namespaces)):
                f.write("}\n")
                namespaces.pop()
            for i in range(j, len(newNamespaces)):
                f.write("namespace %s {\n" % newNamespaces[i])
                namespaces.append(newNamespaces[i])
            f.write("class %s;\n" % type)
        for namespace in namespaces:
            f.write("}\n")
        f.write("\n")

        f.write("namespace mozilla {\n"
                "namespace dom {\n"
                "namespace binding {\n\n")
        f.write("bool\n"
                "DefinePropertyStaticJSVals(JSContext *cx);\n\n")

        for clazz in config.classes.itervalues():
            f.write(string.Template(listDefinitionTemplate).substitute(clazz))

        f.write("\n"
                "}\n"
                "}\n"
                "}\n\n")
        f.write("#endif\n")
    finally:
        f.close()

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

# === Generating the source file

listTemplateHeader = (
"// ${name}\n"
"\n"
"typedef ${listClass} ${name}Class;\n"
"typedef ListBase<${name}Class> ${name}Wrapper;\n"
"\n"
"\n")

listTemplate = (
"// ${name}\n"
"\n"
"template<>\n"
"js::Class ${name}Wrapper::sInterfaceClass = {\n"
"    \"${name}\",\n"
"    0,\n"
"    JS_PropertyStub,        /* addProperty */\n"
"    JS_PropertyStub,        /* delProperty */\n"
"    JS_PropertyStub,        /* getProperty */\n"
"    JS_StrictPropertyStub,  /* setProperty */\n"
"    JS_EnumerateStub,\n"
"    JS_ResolveStub,\n"
"    JS_ConvertStub,\n"
"    JS_FinalizeStub,\n"
"    NULL,                   /* checkAccess */\n"
"    NULL,                   /* call */\n"
"    NULL,                   /* construct */\n"
"    interface_hasInstance\n"
"};\n"
"\n")

derivedClassTemplate = (
"template<>\n"
"bool\n"
"${name}Wrapper::objIsList(JSObject *obj)\n"
"{\n"
"    if (!js::IsProxy(obj))\n"
"        return false;\n"
"    js::ProxyHandler *handler = js::GetProxyHandler(obj);\n"
"    return proxyHandlerIsList(handler) ||\n"
"${checkproxyhandlers};\n"
"}\n"
"\n"
"template<>\n"
"${nativeClass}*\n"
"${name}Wrapper::getNative(JSObject *obj)\n"
"{\n"
"    js::ProxyHandler *handler = js::GetProxyHandler(obj);\n"
"    if (proxyHandlerIsList(handler))\n"
"        return static_cast<${nativeClass}*>(js::GetProxyPrivate(obj).toPrivate());\n"
"${castproxyhandlers}"
"\n"
"    NS_RUNTIMEABORT(\"Unknown list type!\");\n"
"    return NULL;\n"
"}\n"
"\n")

prefableClassTemplate = (
"template<>\n"
"JSObject *\n"
"${name}Wrapper::getPrototype(JSContext *cx, XPCWrappedNativeScope *scope, bool *enabled)\n"
"{\n"
"    if (!scope->NewDOMBindingsEnabled()) {\n"
"        *enabled = false;\n"
"        return NULL;\n"
"    }\n"
"\n"
"    *enabled = true;\n"
"    return getPrototype(cx, scope);\n"
"}\n"
"\n")

toStringTemplate = (
"template<>\n"
"JSString *\n"
"${name}Wrapper::obj_toString(JSContext *cx, JSObject *proxy)\n"
"{\n"
"    nsString result;\n"
"    nsresult rv = ${name}Wrapper::getListObject(proxy)->ToString(result);\n"
"    JSString *jsresult;\n"
"    return NS_SUCCEEDED(rv) && xpc_qsStringToJsstring(cx, result, &jsresult) ? jsresult : NULL;\n"
"}\n"
"\n")

indexGetterTemplate = (
"template<>\n"
"bool\n"
"${name}Wrapper::getItemAt(${nativeClass} *list, uint32_t index, ${indexGetterType} &item)\n"
"{\n"
"${indexGet}"
"}\n"
"\n")

indexSetterTemplate = (
"template<>\n"
"bool\n"
"${name}Wrapper::setItemAt(JSContext *cx, ${nativeClass} *list, uint32_t index, ${indexSetterType} item)\n"
"{\n"
"${indexSet}"
"}\n"
"\n")

nameGetterTemplate = (
"template<>\n"
"bool\n"
"${name}Wrapper::getNamedItem(${nativeClass} *list, const nsAString& index, ${nameGetterType} &item)\n"
"{\n"
"${nameGet}"
"}\n"
"\n")

nameSetterTemplate = (
"template<>\n"
"bool\n"
"${name}Wrapper::setNamedItem(JSContext *cx, ${nativeClass} *list, const nsAString& index, ${nameSetterType} item)\n"
"{\n"
"${nameSet}"
"}\n"
"\n")

propertiesTemplate = (
"template<>\n"
"${name}Wrapper::Properties ${name}Wrapper::sProtoProperties[] = {\n"
"${properties}\n"
"};\n"
"\n"
"template<>\n"
"size_t ${name}Wrapper::sProtoPropertiesCount = ArrayLength(${name}Wrapper::sProtoProperties);\n"
"\n")

methodsTemplate = (
"template<>\n"
"${name}Wrapper::Methods ${name}Wrapper::sProtoMethods[] = {\n"
"${methods}\n"
"};\n"
"\n"
"template<>\n"
"size_t ${name}Wrapper::sProtoMethodsCount = ArrayLength(${name}Wrapper::sProtoMethods);\n"
"\n")

listTemplateFooter = (
"template class ListBase<${name}Class>;\n"
"\n"
"JSObject*\n"
"${name}::create(JSContext *cx, XPCWrappedNativeScope *scope, ${nativeClass} *list, nsWrapperCache *cache, bool *triedToWrap)\n"
"{\n"
"    return ${name}Wrapper::create(cx, scope, list, cache, triedToWrap);\n"
"}\n"
"\n"
"bool\n"
"${name}::objIsWrapper(JSObject *obj)\n"
"{\n"
"    return ${name}Wrapper::objIsList(obj);\n"
"}\n"
"\n"
"${nativeClass}*\n"
"${name}::getNative(JSObject *obj)\n"
"{\n"
"    return ${name}Wrapper::getListObject(obj);\n"
"}\n"
"\n")

def writeBindingStub(f, classname, member, stubName, isSetter=False):
    def writeThisUnwrapping(f, member, isMethod, isGetter, customMethodCall, haveCcx):
        if isMethod:
            f.write("    JSObject *callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));\n"
                    "    if (!%sWrapper::instanceIsListObject(cx, obj, callee))\n"
                    "        return false;\n" % classname)
        else:
            f.write("    if (!%sWrapper::instanceIsListObject(cx, obj, NULL))\n"
                    "        return false;\n" % classname)
        return "%sWrapper::getListObject(obj)" % classname
    def writeCheckForFailure(f, isMethod, isGeter, haveCcx):
        f.write("    if (NS_FAILED(rv))\n"
                "        return xpc_qsThrowMethodFailedWithDetails(cx, rv, \"%s\", \"%s\");\n" % (classname, member.name))
    def writeResultWrapping(f, member, jsvalPtr, jsvalRef):
        if member.kind == 'method' and member.notxpcom and len(member.params) > 0 and member.params[len(member.params) - 1].paramtype == 'out':
            assert member.params[len(member.params) - 1].realtype.kind == 'native' and member.params[len(member.params) - 1].realtype.nativename == 'nsWrapperCache'
            template = "    return Wrap(cx, obj, result, cache, ${jsvalPtr});\n"
        else:
            template = "    return Wrap(cx, obj, result, ${jsvalPtr});\n"
        writeResultConv(f, member.realtype, template, jsvalPtr, jsvalRef)

    writeStub(f, {}, member, stubName, writeThisUnwrapping, writeCheckForFailure, writeResultWrapping, isSetter)

def writeAttrStubs(f, classname, attr):
    getterName = classname + '_' + header.attributeNativeName(attr, True)
    writeBindingStub(f, classname, attr, getterName)
    if attr.readonly:
        setterName = 'xpc_qsGetterOnlyPropertyStub'
    else:
        setterName = (classname + '_'
                      + header.attributeNativeName(attr, False))
        writeBindingStub(f, classname, attr, setterName, isSetter=True)

    return "    { s_%s_id, %s, %s }" % (attr.name, getterName, setterName)

def writeMethodStub(f, classname, method):
    stubName = classname + '_' + header.methodNativeName(method)
    writeBindingStub(f, classname, method, stubName)
    return "    { s_%s_id, %s, %i }" % (method.name, stubName, argumentsLength(method))

def writeStubFile(filename, config, interfaces):
    print "Creating stub file", filename
    make_targets.append(filename)

    f = open(filename, 'w')
    filesIncluded = set()

    headerFilename = re.sub(r'(\.cpp)?$', '.h', filename)

    try:
        f.write("/* THIS FILE IS AUTOGENERATED - DO NOT EDIT */\n\n")

        types = getTypes(config.classes, config.irregularFilenames)
        for clazz in config.classes.itervalues():
            for member in clazz.members:
                addType(types, member.realtype, config.irregularFilenames)
                if member.kind == 'method':
                    for p in member.params:
                        addType(types, p.realtype, config.irregularFilenames)

        f.write("".join([("#include \"%s.h\"\n" % re.sub(r'(([^:]+::)*)', '', type)) for type in sorted(types)]))
        f.write("\n")

        f.write("namespace mozilla {\n"
                "namespace dom {\n"
                "namespace binding {\n\n")

        f.write("// Property name ids\n\n")

        ids = set()
        for clazz in config.classes.itervalues():
            assert clazz.indexGetter
            ids.add(clazz.indexGetter.name)
            if clazz.indexSetter:
                ids.add(clazz.indexSetter.name)
            if clazz.nameGetter:
                ids.add(clazz.nameGetter.name)
            if clazz.stringifier:
                ids.add('toString')
            for member in clazz.members:
                if member.name != 'length':
                    ids.add(member.name)

        ids = sorted(ids)
        for id in ids:
            f.write("static jsid s_%s_id = JSID_VOID;\n" % id)
        f.write("\n"
                "bool\n"
                "DefinePropertyStaticJSVals(JSContext *cx)\n"
                "{\n")
        f.write("    return %s;" % (" &&\n           ".join([("SET_JSID_TO_STRING(cx, %s)" % id) for id in ids])))
        f.write("\n"
                "}\n\n")

        classes = sorted(config.classes.values())

        f.write("// Typedefs\n\n")

        for clazz in classes:
            f.write(string.Template(listTemplateHeader).substitute(clazz))

        f.write("// Implementation\n\n")

        for clazz in classes:
            f.write(string.Template(listTemplate).substitute(clazz))
            derivedClasses = config.derivedClasses.get(clazz.name, None)
            if derivedClasses:
                # If this hits we might need to do something better than just compare instance pointers
                assert len(derivedClasses) <= 3
                checkproxyhandlers = "||\n".join(map(lambda d: "           %sWrapper::proxyHandlerIsList(handler)" % d, derivedClasses))
                castproxyhandlers = "\n".join(map(lambda d: "    if (%sWrapper::proxyHandlerIsList(handler))\n        return %sWrapper::getNative(obj);\n" % (d, d), derivedClasses))
                f.write(string.Template(derivedClassTemplate).substitute(clazz, checkproxyhandlers=checkproxyhandlers, castproxyhandlers=castproxyhandlers))
            if clazz.prefable:
                f.write(string.Template(prefableClassTemplate).substitute(clazz))
            methodsList = []
            propertiesList = []
            if clazz.stringifier:
                f.write(string.Template(toStringTemplate).substitute(clazz))
                if clazz.stringifier.name != 'toString':
                    methodsList.append("    { s_toString_id, %s_%s, 0 }", clazz.name, header.methodNativeName(clazz.stringifier))
            if clazz.indexGetter:
                #methodsList.append("    { s_%s_id, &item, 1 }" % clazz.indexGetter.name)
                f.write(string.Template(indexGetterTemplate).substitute(clazz))
            if clazz.indexSetter:
                f.write(string.Template(indexSetterTemplate).substitute(clazz))
            if clazz.nameGetter:
                #methodsList.append("    { s_%s_id, &namedItem, 1 }" % clazz.nameGetter.name)
                f.write(string.Template(nameGetterTemplate).substitute(clazz))
            if clazz.nameSetter:
                f.write(string.Template(nameSetterTemplate).substitute(clazz))
            for member in sorted(clazz.members, key=lambda member: member.name):
                if member.name == 'length':
                    if not member.readonly:
                        setterName = (clazz.name + '_' + header.attributeNativeName(member, False))
                        writeBindingStub(f, clazz.name, member, setterName, isSetter=True)
                    else:
                        setterName = "NULL"

                    propertiesList.append("    { s_length_id, length_getter, %s }" % setterName)
                    continue

                isAttr = (member.kind == 'attribute')
                isMethod = (member.kind == 'method')
                assert isAttr or isMethod

                if isMethod:
                    methodsList.append(writeMethodStub(f, clazz.name, member))
                else:
                    propertiesList.append(writeAttrStubs(f, clazz.name, member))

            if len(propertiesList) > 0:
                f.write(string.Template(propertiesTemplate).substitute(clazz, properties=",\n".join(propertiesList)))
            if len(methodsList) > 0:
                f.write(string.Template(methodsTemplate).substitute(clazz, methods=",\n".join(methodsList)))
            f.write(string.Template(listTemplateFooter).substitute(clazz))
            
        f.write("// Register prototypes\n\n")

        f.write("void\n"
                "Register(nsDOMClassInfoData *aData)\n"
                "{\n"
                "#define REGISTER_PROTO(_dom_class) \\\n"
                "    aData[eDOMClassInfo_##_dom_class##_id].mDefineDOMInterface = _dom_class##Wrapper::getPrototype\n"
                "\n")
        for clazz in config.classes.itervalues():
            f.write("    REGISTER_PROTO(%s);\n" % clazz.name)
        f.write("\n"
                "#undef REGISTER_PROTO\n"
                 "}\n\n")

        f.write("}\n"
                "}\n"
                "}\n")
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

    if options.cachedir != '':
        sys.path.append(options.cachedir)
        if not os.path.isdir(options.cachedir):
            os.makedirs(options.cachedir)

    try:
        includePath = options.idlpath.split(':')
        conf = readConfigFile(filename,
                              includePath=includePath)
        if options.header_output is not None:
            writeHeaderFile(options.header_output, conf)
        elif options.stub_output is not None:
            interfaces = completeConfiguration(conf,
                                               includePath=includePath,
                                               cachedir=options.cachedir)
            writeStubFile(options.stub_output, conf, interfaces)
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
