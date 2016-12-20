#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generator for C style prototypes and definitions """

import glob
import os
import re
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLNode
from idl_ast import IDLAst
from idl_option import GetOption, Option, ParseOptions
from idl_parser import ParseFiles
from idl_c_proto import CGen
from idl_c_header import GetHeaderFromNode, GenerateHeader
from idl_outfile import IDLOutFile

Option('debug', 'Debug generate.')

#API = namedtuple('API', ['interface', 'member', 'param'])

customAPIs = {
  ('PPB_CharSet_Dev', 'CharSetToUTF16', 'input'): { 'array': True, 'arrayType': 'uint8_t', 'arraySize': 'input_len' },
  ('PPB_CharSet_Dev', 'CharSetToUTF16', 'rval'): { 'convert': '  size_t length = iterator.expectArrayAndGotoFirstItem();\n  rval = new uint16_t[length];\n  for (size_t i = 0; i < length; ++i) {\n    FromJSON_uint16_t(iterator, rval[i]);\n  }\n' },
  ('PPB_Core', 'IsMainThread', None): { 'maybeNonMainThread': True },
  ('PPB_Core', 'CallOnMainThread', None): { 'maybeNonMainThread': True },
  ('PPB_Flash_MessageLoop', 'Quit', None): { 'maybeNonMainThread': True },
  ('PPB_Graphics3D', 'Create', 'attrib_list'): { 'arraySentinel': 'PP_GRAPHICS3DATTRIB_NONE' },
  ('PPB_MediaStreamAudioTrack', 'Configure', 'attrib_list'): { 'arraySentinel': 'PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE' },
  ('PPB_OpenGLES2', 'DeleteBuffers', 'buffers'): { 'array': True, 'arrayType': 'GLuint', 'arraySize': 'n' },
  ('PPB_OpenGLES2', 'DeleteFramebuffers', 'framebuffers'): { 'array': True, 'arrayType': 'GLuint', 'arraySize': 'n' },
  ('PPB_OpenGLES2', 'DeleteRenderbuffers', 'renderbuffers'): { 'array': True, 'arrayType': 'GLuint', 'arraySize': 'n' },
  ('PPB_OpenGLES2', 'DeleteTextures', 'textures'): { 'array': True, 'arrayType': 'GLuint', 'arraySize': 'n' },
  ('PPB_OpenGLES2', 'GenBuffers', 'buffers'): { 'array': True, 'arrayType': 'GLuint', 'arraySize': 'n', 'create': False },
  ('PPB_OpenGLES2', 'GenTextures', 'textures'): { 'array': True, 'arrayType': 'GLuint', 'arraySize': 'n', 'create': False },
  ('PPB_OpenGLES2', 'ShaderSource', 'str'): { 'array': True, 'arrayType': 'str_t', 'arraySize': 'count', 'mode': 'in' },
  ('PPB_OpenGLES2', 'UniformMatrix3fv', 'value'): { 'array': True, 'arrayType': 'GLfloat', 'arraySize': 'count * 9', 'mode': 'in' },
  ('PPB_PDF', 'SearchString', 'results'): { 'arraySize': None },
  ('PPB_PDF', 'SetAccessibilityPageInfo', 'text_runs'): { 'arraySize': 'page_info->text_run_count' },
  ('PPB_PDF', 'SetAccessibilityPageInfo', 'chars'): { 'arraySize': 'page_info->char_count' },
  ('PPB_TCPSocket_Private', 'Write', 'buffer'): { 'array': True, 'arrayType': 'uint8_t', 'arraySize': 'bytes_to_write' },
}
def getCustom(interface, member, param):
  def matches(pattern, value):
    return pattern == value or pattern == '*'
  custom = list(v for (i, m, p), v in customAPIs.iteritems() if matches(interface, i) and matches(m, member) and matches(p, param))
  assert len(custom) <= 1
  return custom[0] if len(custom) == 1 else dict()

class RPCGen(object):
  def __init__(self):
    self.cgen = CGen()
    self.interfaceStructs = []
    self.pppInterfaceGetters = []

  def AddProp(self, release, s, prefix, member, isOutParam=False, callnode=None, derefSizeIs=True):
    out = ''
    key = member.GetName()
    if not member.InReleases([release]):
      out += '/* skipping %s */\n' % key
      return
    type = member.GetType(release).GetName()
    array = member.GetOneOf('Array')
    interfaceMember = member.parent
    interface = interfaceMember.parent.parent
    customSerializer = getCustom(interface.GetName(), interfaceMember.GetName(), member.GetName())
    array = customSerializer.get('array', array)
    isOutParam = customSerializer.get('isOutParam', isOutParam)
    if isOutParam:
      out += '  AddProp(' + s + ', "' + key + '", PointerToString(' + prefix + key + '));\n'
    elif array:
      if customSerializer:
        type = customSerializer.get('arrayType', type)
      count = array.GetProperty('FIXED') if isinstance(array, IDLNode) else None
      if not count:
        size_is = member.GetProperty('size_is()')
        if size_is:
          size_is = size_is[0]
          count = prefix + size_is
          if callnode:
            for param in callnode.GetListOf('Param'):
              if param.GetName() == size_is:
                if self.cgen.GetParamMode(param) == 'out' and derefSizeIs:
                  count = '*' + count
                break
        else:
          size_as = member.GetProperty('size_as')
          count = size_as
      if customSerializer:
        count = customSerializer.get('arraySize', count)
      out += '  {\n'
      out += '    BeginProp(' + s + ', "' + key + '");\n'
      out += '    BeginElements(' + s + ');\n'
      out += '    for (uint32_t _n = 0; '
      if count:
        out += '_n < ' + count
      else:
        sentinel = customSerializer.get('arraySentinel', '0')
        out += prefix + key + '[_n] != ' + sentinel
      out += '; ++_n) {\n'
      out += '      AddElement(' + s + ', ToString_' + type + '(' + prefix + key + '[_n]));\n'
      out += '    }\n'
      out += '    EndElements(' + s + ');\n'
      out += '  }\n'
    else:
      out += '  AddProp(' + s + ', "' + key + '", ToString_' + type + '(' + prefix + key + '));\n'
    return out

  def FromJSON(self, release, prefix, outvalue, node, create, callnode=None):
    out = ''
    type = node.GetType(release).GetName()
    array = node.GetOneOf('Array')
    interfaceMember = node.parent
    interface = interfaceMember.parent.parent
    customParser = getCustom(interface.GetName(), interfaceMember.GetName(), node.GetName())
    array = customParser.get('array', array)
    create = customParser.get('create', create)
    name = node.GetName()
    outvalue = prefix + outvalue
    if array:
      if customParser:
        type = customParser.get('arrayType', type)
      fixed = array.GetProperty('FIXED') if isinstance(array, IDLNode) else None
      count = fixed
      if type == 'char':
        assert count > 0
        out += '  FromJSON_charArray(iterator, ' + outvalue + ', ' + str(count) + ');\n'
      else:
        if not count:
          size_is = node.GetProperty('size_is()')
          if size_is:
            size_is = size_is[0]
            count = prefix + size_is
            if callnode:
              for param in callnode.GetListOf('Param'):
                if param.GetName() == size_is:
                  if self.cgen.GetParamMode(param) == 'out':
                    count = '*' + count
                  break
          else:
            size_as = node.GetProperty('size_as')
            count = size_as
        if customParser:
          count = customParser.get('arraySize', count)
        out += '\n'
        out += '  {\n'
        out += '    size_t children = iterator.expectArrayAndGotoFirstItem();\n'
        if count:
          out += '    if (children > ' + count + ') {\n'
          out += '      Fail("Too many items in array\\n", "");\n'
          out += '    }\n'
        if create and not fixed:
          if not count:
            count = 'children'
          out += '    ' + outvalue + ' = new ' + self.cgen.GetTypeByMode(node, release, 'store') + '[' + count + '];\n'
        out += '    for (uint32_t _n = 0; _n < children; ++_n) {\n'
        out += '      FromJSON_' + type + '(iterator, (' + outvalue + ')[_n]);\n'
        out += '    }\n'
        out += '    // FIXME Null out remaining items?\n'
        out += '  }\n'
    else:
      out += '  FromJSON_' + type + '(iterator, ' + outvalue + ');\n'
    return out

  def MemberFromJSON(self, release, prefix, member, create):
    key = member.GetName()
    if not member.InReleases([release]):
      return '/* skipping %s */\n' % key
    return self.FromJSON(release, prefix, key, member, create)

  DerefOutType = {
    'Array': { 'out': '*' },
    'Enum': '*',
    'Struct': '*',
    'cstr_t': '*',
    'TypeValue': '*',
  }

  def DefineInterfaceMethodSerializer(self, iface, releases, node, release):
    customSerializer = getCustom(iface.GetName(), node.GetName(), None)
    if not node.InReleases([release]):
      return '/* skipping %s */\n' % node.GetName()
    if customSerializer and customSerializer.get('skip', False):
      return self.cgen.GetSignature(node, release, 'ref', '', func_as_ptr=False, include_version=True) + ";\n"
    out = ''
    out += 'static '
    out += self.cgen.GetSignature(node, release, 'ref', '', func_as_ptr=False, include_version=True)
    out += ' {\n'
    out += '  stringstream ss;\n'
    out += '  BeginProps(ss);\n'
    out += '  AddProp(ss, "__interface", "\\"' + iface.GetName() + '\\"");\n'
    out += '  AddProp(ss, "__version", "\\"' + iface.GetVersion(release) + '\\"");\n'
    out += '  AddProp(ss, "__method", "\\"' + node.GetName() + '\\"");\n'
    callnode = node.GetOneOf('Callspec')
    hasOutParams = False
    if callnode:
      for param in callnode.GetListOf('Param'):
        mode = self.cgen.GetParamMode(param)
        ntype, mode = self.cgen.GetRootTypeMode(param, release, mode)
        mode = getCustom(param.parent.parent.parent.GetName(), param.parent.GetName(), param.GetName()).get('mode', mode)
        if mode == "out" or mode == "inout":
          hasOutParams = hasOutParams or not (ntype == 'mem_t' and mode == "inout")
        out += self.AddProp(release, 'ss', '', param, mode == "out", callnode)
    out += '  EndProps(ss);\n'
    if node.GetProperty('ref'):
      mode = 'ref'
    else:
      mode = 'store'
    rtype = self.cgen.GetTypeByMode(node.GetType(release), release, 'store')
    out += '#ifndef INTERPOSE\n'
    out += '  '
    if rtype != 'void' or hasOutParams:
      out += 'string json = RPCWithResult'
    else:
      out += 'RPC'
    if customSerializer and customSerializer.get('maybeNonMainThread', False):
      out += '<MaybeNonMainThread>'
    else:
      out += '<MainThreadOnly>'
    out += '(ss);\n'
    if rtype != 'void' or hasOutParams:
      if rtype != 'void':
        out += '  ' + rtype + ' rval;\n'
      out += '  JSONIterator iterator(json);\n'
      out += '  iterator.expectArrayAndGotoFirstItem();\n'
      if hasOutParams:
        out += '  iterator.expectArrayAndGotoFirstItem();\n'
      if rtype != 'void':
        customRval = getCustom(iface.GetName(), node.GetName(), "rval").get('convert')
        if customRval:
          out += customRval
        else:
          out += '  FromJSON_' +  node.GetType(release).GetName() + '(iterator, rval);\n'
      if hasOutParams:
        out += '  iterator.expectObjectAndGotoFirstProperty();\n'
        for param in callnode.GetListOf('Param'):
          mode = self.cgen.GetParamMode(param)
          ntype, mode = self.cgen.GetRootTypeMode(param, release, mode)
          mode = getCustom(param.parent.parent.parent.GetName(), param.parent.GetName(), param.GetName()).get('mode', mode)
          if mode == "out" or mode == "inout":
            if ntype == 'mem_t' and mode == "inout":
              continue;
            if ntype == 'Struct' and mode == 'out':
              out += '  if (!!' + param.GetName() + ') {\n'
            to_indent = '  iterator.skip();\n'
            deref = self.DerefOutType.get(ntype, '')
            if isinstance(deref, dict):
              deref = deref.get(mode, '')
            to_indent += self.FromJSON(release, '', deref + param.GetName(), param, mode == 'out', callnode)
            if ntype == 'Struct' and mode == 'out':
              out += re.sub(r"(^|\n)(?!(\n|$))", r'\1' + (2 * ' '), to_indent)
              out += '  }\n'
            else:
              out += to_indent
      if rtype != 'void':
        out += '  return rval;\n'
    out += '#else // !INTERPOSE\n'
    out += '  printf("%s\\n", ss.str().c_str());\n'
    #out += '  printf("Calling: %p\\n", RealGetInterface("' + self.cgen.GetInterfaceString(iface, iface.GetVersion(release)) + '"));\n'
    #out += '  printf("  -> %p\\n", ((' + self.cgen.GetStructName(iface, release, include_version=True) + '*)RealGetInterface("' + self.cgen.GetInterfaceString(iface, iface.GetVersion(release)) + '"))->' + node.GetName() + ');\n'
    out += '  '
    rtype = self.cgen.GetTypeByMode(node.GetType(release), release, 'return')
    params = []
    for param in callnode.GetListOf('Param'):
      mode = self.cgen.GetParamMode(param)
      ntype, mode = self.cgen.GetRootTypeMode(param, release, mode)
      if mode == "in" and param.GetType(release).GetName() == 'PP_CompletionCallback':
        out += 'PP_CompletionCallback logging_' + param.GetName() + ';\n'
        out += '  logging_' + param.GetName() + '.func = &Logging_PP_CompletionCallback;\n'
        out += '  logging_' + param.GetName() + '.user_data = new PP_CompletionCallback(' + param.GetName() + ');\n'
        out += '  logging_' + param.GetName() + '.flags = ' + param.GetName() + '.flags;\n'
        out += '  '
        params.append('logging_' + param.GetName())
      elif mode == "in" and param.GetType(release).GetName() == 'PPP_Class_Deprecated':
        out += 'Logging_PPP_Class_Deprecated_holder* logging_' + param.GetName() + ' = new Logging_PPP_Class_Deprecated_holder();\n'
        out += '  logging_' + param.GetName() + '->_real_PPP_Class_Deprecated = ' + param.GetName() + ';\n'
        out += '  logging_' + param.GetName() + '->object = object_data;\n'
        out += '  object_data = logging_' + param.GetName() + ';\n'
        out += '  '
        params.append('&_interpose_PPP_Class_Deprecated_1_0')
      elif mode == "return" and param.GetType(release).GetName() == 'PPB_Audio_Callback' and param.GetVersion(release) == "1.0":
        out += 'Logging_PPB_Audio_Callback_1_0_holder* ' + param.GetName() + '_holder = new Logging_PPB_Audio_Callback_1_0_holder();\n'
        out += '  ' + param.GetName() + '_holder->func = ' + param.GetName() + ';\n'
        out += '  ' + param.GetName() + '_holder->user_data = user_data;\n'
        out += '  user_data = ' + param.GetName() + '_holder;\n'
        out += '  '
        params.append('Logging_PPB_Audio_Callback_1_0')
      else:
        params.append(param.GetName())
    if rtype != 'void':
      out += rtype + ' rval = '
    out += '((' + self.cgen.GetStructName(iface, release, include_version=True) + '*)RealGetInterface("' + self.cgen.GetInterfaceString(iface, iface.GetVersion(release)) + '"))->' + node.GetName() + '(' + ', '.join(params) + ');\n'
    if rtype != 'void' or hasOutParams:
      out += '  printf("RPC response: [");\n'
      if hasOutParams:
        out += '  printf("[");\n'
      if rtype != 'void':
        out += '  printf("%s", ToString_' +  node.GetType(release).GetName() + '(rval).c_str());\n'
      if hasOutParams:
        if rtype != 'void':
          out += '  printf(",");\n'
        out += '  std::stringstream os;\n'
        out += '  BeginProps(os);\n'
        for param in callnode.GetListOf('Param'):
          mode = self.cgen.GetParamMode(param)
          ntype, mode = self.cgen.GetRootTypeMode(param, release, mode)
          if mode == "out" or mode == "inout":
            if mode == "out" and (ntype == 'Struct' or ntype == 'TypeValue'):
              out += '  if (!!' + param.GetName() + ') {\n'
            out += self.AddProp(release, 'os', '', param, False, callnode)
            if mode == "out" and (ntype == 'Struct' or ntype == 'TypeValue'):
              out += '  }\n'
        out += '  EndProps(os);\n'
        out += '  printf("%s]", os.str().c_str());\n'
      out += '  printf("]\\n");\n'
      if rtype != 'void':
        out += '  return rval;\n'
    out += '#endif // !INTERPOSE\n'
    out += '}\n'
    return out

  def DefineInterfaceMethodParser(self, iface, releases, node, release):
    if not node.InReleases([release]):
      return '/* skipping %s */\n' % node.GetName()
    version = self.GetNodeName(iface, release, releases)
    out = ''
    out += 'char* Call_%s_%s(const %s* _interface, JSONIterator& iterator) {\n' % (iface.GetName(), node.GetName(), version)
    callnode = node.GetOneOf('Callspec')
    params = []
    hasOutParams = False
    for param in callnode.GetListOf('Param'):
      mode = self.cgen.GetParamMode(param)
      ptype, pname, parray, pspec = self.cgen.GetComponents(param, release, "store")
      out += '  ' + self.cgen.Compose(ptype, pname, parray, pspec, '', func_as_ptr=True,
              include_name=True, unsized_as_ptr=True) + ';\n'
      if mode == 'out':
        if len(parray) > 0:
          out += '  iterator.skip();\n'
          out += '  PointerValueFromJSON(iterator, ' + pname + ');\n'
      else:
        out += '  iterator.skip();\n'
        out += self.FromJSON(release, '', pname, param, True, callnode)
      hasOutParams = hasOutParams or mode == "out" or mode == "inout"
    if node.GetProperty('ref'):
      mode = 'ref'
    else:
      mode = 'store'
    rtype = self.cgen.GetTypeByMode(node.GetType(release), release, 'store')
    out += '  '
    if rtype != 'void':
      out += rtype + ' rval;\n'
      out += '  rval = '
    params = []
    for param in callnode.GetListOf('Param'):
      mode = self.cgen.GetParamMode(param)
      ntype, mode = self.cgen.GetRootTypeMode(param, release, mode)
      ptype, pname, parray, pspec = self.cgen.GetComponents(param, release, mode)
      if mode == 'out' or ntype == 'Struct' or (mode == 'constptr_in' and ntype == 'TypeValue'):
        pname = '&' + pname
      pname = '(' + self.cgen.Compose(ptype, pname, parray, pspec, '', func_as_ptr=True,
              include_name=False, unsized_as_ptr=True) + ')' + pname
      params.append(pname)
    out += '_interface->' + node.GetName() + '(' + ", ".join(params) + ');\n'
    if rtype != 'void' or hasOutParams:
      typeref = node.GetType(release)
      if hasOutParams:
        out += '  stringstream os;\n'
        out += '  BeginElements(os);\n'
        if rtype != 'void':
          out += '  AddElement(os, ToString_' + typeref.GetName() + '(rval).c_str());\n'
          out += '  BeginElement(os);\n'
        out += '  BeginProps(os);\n'
        for param in callnode.GetListOf('Param'):
          mode = self.cgen.GetParamMode(param)
          ntype, mode = self.cgen.GetRootTypeMode(param, release, mode)
          ptype, pname, parray, pspec = self.cgen.GetComponents(param, release, mode)
          if mode == 'out' or mode == 'inout':
            out += self.AddProp(release, 'os', '', param, False, callnode, derefSizeIs=False)
        out += '  EndProps(os);\n'
        out += '  EndElements(os);\n'
        out += '  return strdup(os.str().c_str());\n'
      else:
        out += '  return strdup(ToString_' + typeref.GetName() + '(rval).c_str());\n'
    else:
      out += '  return nullptr;\n'
    out += '}\n'
    return out

  def DefineInterface(self, node, releases, declareOnly):
    out = ''
    if node.GetName() == "PPB_NaCl_Private":
      # skip
      return out
    isPPP = node.GetName()[0:4] == "PPP_"
    build_list = node.GetUniqueReleases(releases)
    for release in build_list:
      name = self.cgen.GetStructName(node, release, include_version=True)
      if declareOnly:
        out += '#ifdef INTERPOSE\n'
        out += 'static ' + name + ' *_real_' + name + ';\n'
        out += '#endif // INTERPOSE\n'
      if isPPP:
        version = self.GetNodeName(node, release, build_list)
        if declareOnly:
          out += 'static char* Call_%s(void* _interface, JSONIterator& iterator);\n' % (version)
          continue
        members = node.GetListOf('Member')
        for member in members:
          out += self.DefineInterfaceMethodParser(node, build_list, member, release)
        out += 'char* Call_%s(const void* _interface, JSONIterator& iterator) {\n' % (version)
        out += '  iterator.skip();\n'
        out += '  const Token& member = iterator.getCurrentStringAndGotoNext();\n'
        out += '  string memberName = member.value();\n'
        for member in members:
          if not member.InReleases([release]):
            out += '/* skipping %s */\n' % member.GetName()
            continue
          out += '  if (!memberName.compare("' + member.GetName() + '")) {\n'
          out += '    return Call_' + node.GetName() + '_' + member.GetName() + '((const ' + version + '*)_interface, iterator);\n'
          out += '  }\n'
        out += '  return nullptr;\n'
        out += '}\n'
        self.pppInterfaceGetters.append((self.cgen.GetInterfaceString(node, node.GetVersion(release)), 'Call_' + version))
        continue
      version = self.cgen.GetStructName(node, release, include_version=True)
      if declareOnly:
        out += 'const string ToString_%s(const %s *v);\n' % (node.GetName(), version)
        continue
      ns = 'ns_' + version
      out += 'namespace ' + ns + ' {\n'
      members = node.GetListOf('Member')
      for member in members:
        out += self.DefineInterfaceMethodSerializer(node, releases, member, release)
      out += '}\n'
      self.interfaceStructs.append((self.cgen.GetInterfaceString(node, node.GetVersion(release)), '_' + name))
      out += 'static ' + name + ' _' + name + ' = {\n'
      for member in members:
        if not member.InReleases([release]):
          continue
        memberName = self.cgen.GetStructName(member, release, True)
        out += '  ' + ns + '::' + memberName + ',\n'
      out += '};\n'
      out += 'const string ToString_%s(const %s *v) {\n' % (node.GetName(), version)
      out += '  stringstream s;\n'
      out += '  s << v;\n'
      out += '  return s.str();\n'
      out += '}\n'
    return out

  def GetNodeName(self, node, release, build_list):
    return self.cgen.GetStructName(node, release, include_version=(release != build_list[-1]))

  @staticmethod
  def SerializerAndParserSignatures(typename, type):
      s = ('const string ToString_%s(const %s *v)',
           'const string ToString_%s(const %s &v)',
           'void FromJSON_%s(JSONIterator& iterator, %s &value)')
      return (sig % (typename, type) for sig in s)

  def DefineTypedefSerializerAndParser(self, node, releases, declareOnly):
    out = ''
    build_list = node.GetUniqueReleases(releases)
    for release in build_list:
      type = self.GetNodeName(node, release, build_list)
      typeref = node.GetType(release)
      (toStringFromPointer, toStringFromRef, fromJSONToRef) = self.SerializerAndParserSignatures(node.GetName(), type)
      isFuncPtr = node.GetOneOf('Callspec')
      if declareOnly:
        if not isFuncPtr:
          out += toStringFromPointer + ";\n"
        out += toStringFromRef + ";\n"
        out += fromJSONToRef + ";\n"
        continue
      if not isFuncPtr:
        out += toStringFromPointer + ' {\n'
        out += '  return ToString_%s(v);\n' % (typeref.GetName())
        out += '}\n'
      out += toStringFromRef + ' {\n'
      if isFuncPtr:
        out += '  return PointerToString(v);\n'
      else:
        out += '  return ToString_%s(&v);\n' % (node.GetName())
      out += '}\n'
      out += fromJSONToRef + ' {\n'
      if isFuncPtr:
        out += '  PointerValueFromJSON(iterator, value);\n'
      else:
        out += '  FromJSON_%s(iterator, value);\n' % (typeref.GetName())
      out += '}\n'
    return out

  def DefineStructSerializerAndParser(self, node, releases, declareOnly):
    out = ''
    build_list = node.GetUniqueReleases(releases)
    for release in build_list:
      name = node.GetName()
      version = self.GetNodeName(node, release, build_list)
      (toStringFromPointer, toStringFromRef, fromJSONToRef) = self.SerializerAndParserSignatures(name, version)
      if declareOnly:
        out += toStringFromPointer + ";\n"
        out += toStringFromRef + ";\n"
        out += fromJSONToRef + ";\n"
        continue
      out += toStringFromPointer + ' {\n'
      out += '  if (!v) {\n'
      out += '    return "null";\n'
      out += '  }\n'
      out += '  return ToString_%s(*v);\n' % (name)
      out += '}\n'
      out += toStringFromRef + ' {\n'
      out += '  stringstream x;\n'
      out += '  BeginProps(x);\n'
      members = node.GetListOf('Member')
      for member in members:
        out += self.AddProp(release, 'x', 'v.', member)
      out += '  EndProps(x);\n'
      out += '  return x.str();\n'
      out += '}\n'
      out += fromJSONToRef + ' {\n'
      out += '  const JSON::Token& current = iterator.getCurrentAndGotoNext();\n'
      # FIXME Should we warn here somehow? It might be ok to return null in
      #       error conditions, so maybe not.
      out += '  if (current.isPrimitive() && !current.value().compare("null")) {\n'
      out += '    return;\n'
      out += '  }\n'
      out += '  if (!current.isObject()) {\n'
      out += '    Fail("Expected object!", "");\n'
      out += '  }\n'
      if node.GetProperty('union'):
        out += '  string name = iterator.getCurrentStringAndGotoNext().value();\n'
        out += " else ".join(map(lambda m: '  if (!name.compare(\"' + m.GetName() + '\")) {\n  ' + self.MemberFromJSON(release, 'value.', m, False) + '  }', members)) + "\n"
      else:
        for member in members:
          typeref = member.GetType(release)
          out += '  iterator.skip();\n'
          out += self.MemberFromJSON(release, 'value.', member, False)
      out += '}\n'
    return out

  def DefineEnumSerializerAndParser(self, node, releases, declareOnly):
    if node.GetProperty('unnamed'):
      return ''
    out = ''
    name = node.GetName()
    (toStringFromPointer, toStringFromRef, fromJSONToRef) = self.SerializerAndParserSignatures(name, name)
    if declareOnly:
      out += toStringFromPointer + ";\n"
      out += toStringFromRef + ";\n"
      out += fromJSONToRef + ";\n"
      return out
    out += toStringFromPointer + ' {\n'
    out += '  switch (*v) {\n'
    next = 0
    emitted = set()
    for child in node.GetListOf('EnumItem'):
      value = child.GetName()
      label = child.GetProperty('VALUE')
      if not label:
        label = str(next)
        next = next + 1
      if label in emitted:
        continue
      emitted.add(label)
      emitted.add(value)
      out += '    case ' + label + ':\n'
      out += '      return "\\"' + value + '\\"";\n'
    out += '    default:\n'
    out += '      return "\\"???\\"";\n'
    out += '  }\n'
    out += '}\n'
    out += toStringFromRef + ' {\n'
    out += '  return ToString_%s(&v);\n' % (name)
    out += '}\n'
    out += fromJSONToRef + ' {\n'
    out += '  long int v;\n'
    out += '  FromJSON_int(iterator, v);\n'
    out += '  value = %s(v);\n' % (name)
    out += '}\n'
    return out

  def Define(self, node, releases, declareOnly):
    # Skip if this node is not in this release
    if not node.InReleases(releases):
      return "/* skipping %s */\n" % node
    if node.IsA('Typedef'):
      return self.DefineTypedefSerializerAndParser(node, releases, declareOnly)
    if node.IsA('Struct'):
      return self.DefineStructSerializerAndParser(node, releases, declareOnly)
    if node.IsA('Enum'):
      return self.DefineEnumSerializerAndParser(node, releases, declareOnly)
    if node.IsA('Interface'):
      return self.DefineInterface(node, releases, declareOnly)
    return ''

def GetIncludes(f, releases):
  deps = set()
  for release in releases:
    deps |= f.GetDeps(release)
  includes = set([])
  for dep in deps:
    depfile = dep.GetProperty('FILE')
    if depfile:
      includes.add(depfile)
  return includes

def GenerateDep(outfile, emitted, f, releases):
  includes = GetIncludes(f, releases)
  for include in includes:
    if include in emitted:
      continue
    emitted.add(include)
    GenerateDep(outfile, emitted, include, releases)
    outfile.Write('/* ' + include.GetName() + ' */')
    GenerateHeader(outfile, include, releases)

def main(args):
  filenames = ParseOptions(args)
  ast = ParseFiles(filenames)
  rpcgen = RPCGen()
  files = ast.GetListOf('File')
  outname = GetOption('out')
  if outname == '':
    outname = 'out.cc'
  outfile = IDLOutFile(outname)
  emitted = set()
  for f in files:
    if f.GetName() == 'pp_macros.idl':
      GenerateDep(outfile, emitted, f, ast.releases)
  for f in files:
    GenerateDep(outfile, emitted, f, ast.releases)
  out = ''
  out += '#include "host/rpc.h"\n'
  out += '#include "json/json.cpp"\n'
  out += 'using namespace std;\n'
  out += 'using namespace JSON;\n'
  for declareOnly in [True,False]:
    for f in files:
      if f.GetProperty('ERRORS') > 0:
        print 'Skipping %s' % f.GetName()
        continue
      for node in f.GetChildren()[2:]:
        out += rpcgen.Define(node, ast.releases, declareOnly)
  out += 'static map<string, void*> gInterfaces;\n'
  out += '\n'
  out += 'typedef char* (*InterfaceMemberCallFunc)(const void*, JSONIterator&);\n'
  out += 'static map<string, InterfaceMemberCallFunc> gInterfaceMemberCallers;\n'
  out += '\n'
  out += 'void InitializeInterfaceList() {\n'
  for (interfaceString, interfaceStruct) in rpcgen.interfaceStructs:
    if interfaceString == "PPB_Flash_File_FileRef;2.0":
      interfaceString = "PPB_Flash_File_FileRef;2"
    elif interfaceString == "PPB_Flash_File_ModuleLocal;3.0":
      interfaceString = "PPB_Flash_File_ModuleLocal;3"
    elif interfaceString == "PPB_PDF;0.1":
      interfaceString = "PPB_PDF;1"
    out += '  gInterfaces.insert(pair<string, void*>("' + interfaceString + '", &' + interfaceStruct + '));\n'
  for (interfaceString, caller) in rpcgen.pppInterfaceGetters:
    out += '  gInterfaceMemberCallers.insert(pair<string, InterfaceMemberCallFunc>("' + interfaceString + '", ' + caller + '));\n'
  out += '};\n'
  out += '\n'
  outfile.Write(out);
  outfile.Close()

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
