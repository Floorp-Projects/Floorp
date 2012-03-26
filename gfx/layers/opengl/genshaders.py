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
# The Original Code is Mozilla Foundation code.
#
# The Initial Developer of the Original Code is
#   Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Vladimir Vukicevic <vladimir@pobox.com>
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


import sys
import re
import string

defines = dict()

def parseShaderName(shadername):
  name = ""
  params = {}
  inparams = None
  inparam = None
  curparams = []
  for c in shadername:
    if c == '<':
      inparams = ''
    elif c == ':':
      if inparams is None:
        raise Exception(": in shader name")
      inparam = ''
    elif c == ',':
      if inparams is None:
        raise Exception(", in shader name")
      if inparam is None:
        raise Exception("no values for parameter " + inparams)
      curparams.append(inparam)
      inparam = ''
    elif c == '>':
      if inparams is None:
        raise Exception("> in shader name")
      if inparam is None:
        raise Exception("no values for parameter " + inparams)
      curparams.append(inparam)
      params[inparams] = curparams
      name += '$' + inparams + '$'
      inparams = None
      inparam = None
    else:
      if inparam is not None:
        inparam += c
      elif inparams is not None:
        inparams += c
      else:
        name += c
  return (name, params)

def emitShader(fp, shadername, shaderlines):
    (parsedname, params) = parseShaderName(shadername)
    eolContinue = "\\n\\\n";
    pvals = ['']
    pname = ''
    pnames = params.keys()
    if len(pnames) > 1:
      raise Exception("Currently only supports zero or one parameters to a @shader")
    if pnames:
      pname = pnames[0]
      pvals = params[pname]
    for pval in pvals:
      name = parsedname.replace('$' + pname + '$', pval, 1);
      fp.write("static const char %s[] = \"/* %s */%s" % (name,name,eolContinue))
      for line in shaderlines:
          line = line.replace("\\", "\\\\")
          while line.find('$') != -1:
              expansions = re.findall('\$\S+\$', line)
              for m in expansions:
                  mkey = m[1:-1]
                  mkey = mkey.replace('<' + pname + '>', '<' + pval + '>')
                  if not defines.has_key(mkey):
                      print "Error: Undefined expansion used: '%s'" % (m,)
                      sys.exit(1)
                  mval = defines[mkey]
                  if type(mval) == str:
                      line = line.replace(m, mval)
                  elif type(mval) == list:
                      line = line.replace(m, eolContinue.join(mval) + eolContinue);
                  else:
                      print "Internal Error: Unknown type in defines array: '%s'" % (str(type(mval)),)

          fp.write("%s%s" % (line,eolContinue))
      fp.write("\";\n\n");

def genShaders(infile, outfile):
    source = open(infile, "r").readlines()
    desthdr = open(outfile, "w+")

    desthdr.write("/* AUTOMATICALLY GENERATED from " + infile + " */\n");
    desthdr.write("/* DO NOT EDIT! */\n\n");

    global defines

    indefine = None
    inshader = None

    inblock = False
    linebuffer = []

    for line in source:
        # strip comments, if not inblock
        if not inblock and line.startswith("//"):
            continue
        line = string.strip(line)

        if len(line) == 0:
            continue

        if line[0] == '@':
            cmd = line
            rest = ''

            if line.find(' ') != -1:
                cmd = line[0:line.find(' ')]
                rest = string.strip(line[len(cmd) + 1:])
                
            if cmd == "@define":
                if inblock:
                    raise Exception("@define inside a block!")
                space = rest.find(' ')
                if space != -1:
                    defines[rest[0:space]] = rest[space+1:]
                else:
                    indefine = rest
                    inblock = True
            elif cmd == "@shader":
                if inblock:
                    raise Exception("@shader inside a block!")
                if len(rest) == 0:
                    raise Exception("@shader without a name!")
                inshader = rest
                inblock = True
            elif cmd == "@end":
                if indefine is not None:
                    if type(linebuffer) == list:
                        for i in range(len(linebuffer)):
                            linebuffer[i] = linebuffer[i].replace("\\", "\\\\")
                    defines[indefine] = linebuffer
                elif inshader is not None:
                    emitShader(desthdr, inshader, linebuffer)
                else:
                    raise Exception("@end outside of a block!")
                indefine = None
                inshader = None
                inblock = None
                linebuffer = []
            else:
                raise Exception("Unknown command: %s" % (cmd,))
        else:
            if inblock:
                linebuffer.append(line)

if (len(sys.argv) != 3):
    print "Usage: %s infile.txt outfile.h" % (sys.argv[0],)
    sys.exit(1)

genShaders(sys.argv[1], sys.argv[2])
