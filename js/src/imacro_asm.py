#!/usr/bin/env python
# -*- Mode: Python; tab-width: 4; indent-tabs-mode: nil -*-
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
# The Original Code is the TraceMonkey IMacro Assembler.
#
# The Initial Developer of the Original Code is
# Brendan Eich <brendan@mozilla.org>.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

# An imacro (interpreter-macro) assembler in Python.
#
# Filename suffix conventions, used by Makefile.in rules:
#   .jsasm     SpiderMonkey JS assembly source, which could be input to other
#              assemblers than imacro_asm.js, hence the generic suffix!
#   .c.out     C source output by imacro_asm.js

import re
import os

class Op:
    def __init__(self, jsop, opcode, opname, opsrc, oplen, pops, pushes, precedence, flags):
        self.jsop = jsop
        self.opcode = opcode
        self.opname = opname
        self.opsrc = opsrc
        self.oplen = oplen
        self.pops = pops
        self.pushes = pushes
        self.precedence = precedence
        self.flags = flags

def readFileLines(filename):
    f = open(filename)
    try:
        return f.readlines()
    finally:
        f.close()

def load_ops(filename):
    opdef_regexp = re.compile(r'''(?x)
        ^ OPDEF \( (JSOP_\w+),         \s*  # op
                   ([0-9]+),           \s*  # val
                   ("[^"]+" | [\w_]+), \s*  # name
                   ("[^"]+" | [\w_]+), \s*  # image
                   (-1|[0-9]+),        \s*  # len
                   (-1|[0-9]+),        \s*  # uses
                   (-1|[0-9]+),        \s*  # defs
                   ([0-9]+),           \s*  # prec
                   ([\w_| ]+)          \s*  # format
                \) \s* $''')

    def decode_string_expr(expr):
        if expr == 'NULL':
            return None
        if expr[0] == '"':
            assert expr[-1] == '"'
            return expr[1:-1]
        assert expr.startswith('js_') and expr.endswith('_str')
        return expr[3:-4]

    opinfo = []
    for lineno, line in enumerate(readFileLines(filename)):
        if line.startswith('OPDEF'):
            m = opdef_regexp.match(line)
            if m is None:
                raise ValueError("OPDEF line of wrong format in jsopcode.tbl at line %d" % (lineno + 1))
            jsop, opcode, opname, opsrc, oplen, pops, pushes, precedence, flags = m.groups()
            assert int(opcode) == len(opinfo)
            opinfo.append(Op(jsop, int(opcode), decode_string_expr(opname),
                             decode_string_expr(opsrc), int(oplen), int(pops), int(pushes),
                             int(precedence), flags.replace(' ', '').split('|')))
    return opinfo

opinfo = load_ops(os.path.join(os.path.dirname(__file__), "jsopcode.tbl"))
opname2info = dict((info.opname, info) for info in opinfo)
jsop2opcode = dict((info.jsop, info.opcode) for info in opinfo)

def to_uint8(s):
    try:
        n = int(s)
    except ValueError:
        n = -1
    if 0 <= n < (1<<8):
        return n
    raise ValueError("invalid 8-bit operand: " + s)

def to_uint16(s):
    try:
        n = int(s)
    except ValueError:
        n = -1
    if 0 <= n < (1<<16):
        return n
    raise ValueError("invalid 16-bit operand: " + s)

def immediate(op):
    info = op.info
    imm1Expr = op.imm1.startswith('(')
    if 'JOF_ATOM' in info.flags:
        if op.imm1 in ('void', 'object', 'function', 'string', 'number', 'boolean'):
            return "0, COMMON_TYPE_ATOM_INDEX(JSTYPE_%s)" % op.imm1.upper()
        return "0, COMMON_ATOM_INDEX(%s)" % op.imm1
    if 'JOF_JUMP' in info.flags:
        assert not imm1Expr
        return "%d, %d" % ((op.target >> 8) & 0xff, op.target & 0xff)
    if 'JOF_UINT8' in info.flags or 'JOF_INT8' in info.flags:
        if imm1Expr:
            return op.imm1
        return str(to_uint8(op.imm1))
    if 'JOF_UINT16' in info.flags:
        if imm1Expr:
            return '(%s & 0xff00) >> 8, (%s & 0xff)' % (op.imm1, op.imm1)
        v = to_uint16(op.imm1)
        return "%d, %d" % ((v & 0xff00) >> 8, v & 0xff)
    raise NotImplementedError(info.jsop + " format not yet implemented")

def simulate_cfg(igroup, imacro, depth, i):
    any_group_opcode = None
    expected_depth = None
    for opcode in igroup.ops:
        opi = opinfo[opcode]
        if any_group_opcode is None:
            any_group_opcode = opcode
            if opi.pops < 0:
                expected_depth = None
            else:
                expected_depth = opi.pushes - opi.pops
        elif expected_depth is None:
            if opi.pops >= 0:
                raise ValueError("imacro shared by constant- and variable-stack-defs/uses instructions")
        else:
            if opi.pops < 0:
                raise ValueError("imacro shared by constant- and variable-stack-defs/uses instructions")
            if opi.pushes - opi.pops != expected_depth:
                raise ValueError("imacro shared by instructions with different stack depths")

    for i in range(i, len(imacro.code)):
        op = imacro.code[i]
        opi = op.info
        if opi.opname == 'imacop':
            opi = opinfo[any_group_opcode]

        if opi.pops < 0:
            depth -= 2 + int(op.imm1)
        else:
            depth -= opi.pops
        depth += opi.pushes

        if i in imacro.depths and imacro.depths[i] != depth:
            raise ValueError("Mismatched depth at %s:%d" % (imacro.filename, op.line))

        # Underflowing depth isn't necessarily fatal; most of the imacros
        # assume they are called with N>0 args so some assume it's ok to go
        # to some depth <N. We simulate starting from 0, as we've no idea
        # what else to do.
        #
        # if depth < 0:
        #     raise ValueError("Negative static-stack depth at %s:%d" % (imacro.filename, op.line))
        if depth > imacro.maxdepth:
            imacro.maxdepth = depth
        imacro.depths[i] = depth

        if hasattr(op, "target_index"):
            if op.target_index <= i:
                raise ValueError("Backward jump at %s:%d" % (imacro.filename, op.line))
            simulate_cfg(igroup, imacro, depth, op.target_index)
            if op.info.opname in ('goto', 'gotox'):
                return

    if expected_depth is not None and depth != expected_depth:
        raise ValueError("Expected depth %d, got %d" % (expected_depth, depth))


# Syntax (spaces are significant only to delimit tokens):
#
#   Assembly   ::= (Directive? '\n')*
#   Directive  ::= (name ':')? Operation
#   Operation  ::= opname Operands?
#   Operands   ::= Operand (',' Operand)*
#   Operand    ::= name | number | '(' Expr ')'
#   Expr       ::= a constant-expression in the C++ language
#                  containing no parentheses
#
# We simplify given line structure and the maximum of one immediate operand,
# by parsing using split and regexps.  For ease of parsing, parentheses are
# banned in an Expr for now, even in quotes or a C++ comment.
#
# Pseudo-ops start with . and include .igroup and .imacro, terminated by .end.
# .imacro must nest in .igroup, neither nests in itself. See imacros.jsasm for
# examples.
#
line_regexp = re.compile(r'''(?x)
    ^
    (?: (\w+):                     )?  # optional label at start of line
        \s* (\.?\w+)                   # optional spaces, (pseudo-)opcode
    (?: \s+ ([+-]?\w+ | \([^)]*\)) )?  # optional first immediate operand
    (?: \s+ ([\w,-]+  | \([^)]*\)) )?  # optional second immediate operand
    (?: \s* (?:\#.*)               )?  # optional spaces and comment
    $''')

oprange_regexp = re.compile(r'^\w+(?:-\w+)?(?:,\w+(?:-\w+)?)*$')

class IGroup(object):
    def __init__(self, name, ops):
        self.name = name
        self.ops = ops
        self.imacros = []

class IMacro(object):
    def __init__(self, name, filename):
        self.name = name
        self.offset = 0
        self.code = []
        self.labeldefs = {}
        self.labeldef_indexes = {}
        self.labelrefs = {}
        self.filename = filename
        self.depths = {}
        self.initdepth = 0

class Instruction(object):
    def __init__(self, offset, info, imm1, imm2, lineno):
        self.offset = offset
        self.info = info
        self.imm1 = imm1
        self.imm2 = imm2
        self.lineno = lineno

def assemble(filename, outfile):
    write = outfile.write
    igroup = None
    imacro = None
    opcode2extra = {}
    igroups = []

    write("/* GENERATED BY imacro_asm.js -- DO NOT EDIT!!! */\n")

    def fail(msg, *args):
        raise ValueError("%s at %s:%d" % (msg % args, filename, lineno + 1))

    for lineno, line in enumerate(readFileLines(filename)):
        # strip comments
        line = re.sub(r'#.*', '', line).rstrip()
        if line == "":
            continue
        m = line_regexp.match(line)
        if m is None:
            fail(line)

        label, opname, imm1, imm2 = m.groups()

        if opname.startswith('.'):
            if label is not None:
                fail("invalid label %s before %s" % (label, opname))

            if opname == '.igroup':
                if imm1 is None:
                    fail("missing .igroup name")
                if igroup is not None:
                    fail("nested .igroup " + imm1)
                if oprange_regexp.match(imm2) is None:
                    fail("invalid igroup operator range " + imm2)

                ops = set()
                for current in imm2.split(","):
                    split = current.split('-')
                    opcode = jsop2opcode[split[0]]
                    if len(split) == 1:
                        lastopcode = opcode
                    else:
                        assert len(split) == 2
                        lastopcode = jsop2opcode[split[1]]
                        if opcode >= lastopcode:
                            fail("invalid opcode range: " + current)
                        
                    for opcode in range(opcode, lastopcode + 1):
                        if opcode in ops:
                            fail("repeated opcode " + opinfo[opcode].jsop)
                        ops.add(opcode)

                igroup = IGroup(imm1, ops)

            elif opname == '.imacro':
                if igroup is None:
                    fail(".imacro outside of .igroup")
                if imm1 is None:
                    fail("missing .imacro name")
                if imacro:
                    fail("nested .imacro " + imm1)
                imacro = IMacro(imm1, filename)

            elif opname == '.fixup':
                if imacro is None:
                    fail(".fixup outside of .imacro")
                if len(imacro.code) != 0:
                    fail(".fixup must be first item in .imacro")
                if imm1 is None:
                    fail("missing .fixup argument")
                try:
                    fixup = int(imm1)
                except ValueError:
                    fail(".fixup argument must be a nonzero integer")
                if fixup == 0:
                    fail(".fixup argument must be a nonzero integer")
                if imacro.initdepth != 0:
                    fail("more than one .fixup in .imacro")
                imacro.initdepth = fixup

            elif opname == '.end':
                if imacro is None:
                    if igroup is None:
                        fail(".end without prior .igroup or .imacro")
                    if imm1 is not None and (imm1 != igroup.name or imm2 is not None):
                        fail(".igroup/.end name mismatch")

                    maxdepth = 0

                    write("static struct {\n")
                    for imacro in igroup.imacros:
                        write("    jsbytecode %s[%d];\n" % (imacro.name, imacro.offset))
                    write("} %s_imacros = {\n" % igroup.name)

                    for imacro in igroup.imacros:
                        depth = 0
                        write("    {\n")
                        for op in imacro.code:
                            operand = ""
                            if op.imm1 is not None:
                                operand = ", " + immediate(op)
                            write("/*%2d*/  %s%s,\n" % (op.offset, op.info.jsop, operand))

                        imacro.maxdepth = imacro.initdepth
                        simulate_cfg(igroup, imacro, imacro.initdepth, 0)
                        if imacro.maxdepth > maxdepth:
                            maxdepth = imacro.maxdepth

                        write("    },\n")
                    write("};\n")

                    for opcode in igroup.ops:
                        opcode2extra[opcode] = maxdepth
                    igroups.append(igroup)
                    igroup = None
                else:
                    assert igroup is not None

                    if imm1 is not None and imm1 != imacro.name:
                        fail(".imacro/.end name mismatch")

                    # Backpatch the forward references to labels that must now be defined.
                    for label in imacro.labelrefs:
                        if label not in imacro.labeldefs:
                            fail("label " + label + " used but not defined")
                        link = imacro.labelrefs[label]
                        assert link >= 0
                        while True:
                            op = imacro.code[link]
                            next = op.target
                            op.target = imacro.labeldefs[label] - op.offset
                            op.target_index = imacro.labeldef_indexes[label]
                            if next < 0:
                                break
                            link = next

                    igroup.imacros.append(imacro)
                imacro = None

            else:
                fail("unknown pseudo-op " + opname)
            continue

        if opname not in opname2info:
            fail("unknown opcode " + opname)

        info = opname2info[opname]
        if info.oplen == -1:
            fail("unimplemented opcode " + opname)

        if imacro is None:
            fail("opcode %s outside of .imacro", opname)

        # Blacklist ops that may or must use an atomized double immediate.
        if info.opname in ('double', 'lookupswitch', 'lookupswitchx'):
            fail(op.opname + " opcode not yet supported")

        if label:
            imacro.labeldefs[label] = imacro.offset
            imacro.labeldef_indexes[label] = len(imacro.code)

        op = Instruction(imacro.offset, info, imm1, imm2, lineno + 1)
        if 'JOF_JUMP' in info.flags:
            if imm1 in imacro.labeldefs:
                # Backward reference can be resolved right away, no backpatching needed.
                op.target = imacro.labeldefs[imm1] - op.offset
                op.target_index = imacro.labeldef_indexes[imm1]
            else:
                # Link op into the .target-linked backpatch chain at labelrefs[imm1].
                # The linked list terminates with a -1 sentinel.
                if imm1 in imacro.labelrefs:
                    op.target = imacro.labelrefs[imm1]
                else:
                    op.target = -1
                imacro.labelrefs[imm1] = len(imacro.code)

        imacro.code.append(op)
        imacro.offset += info.oplen

    write("uint8 js_opcode2extra[JSOP_LIMIT] = {\n")
    for i in range(len(opinfo)):
        write("    %d,  /* %s */\n" % (opcode2extra.get(i, 0), opinfo[i].jsop))
    write("};\n")

    write("#define JSOP_IS_IMACOP(x) (0 \\\n")
    for i in sorted(opcode2extra):
        write(" || x == %s \\\n" % opinfo[i].jsop)
    write(")\n")

    write("jsbytecode*\njs_GetImacroStart(jsbytecode* pc) {\n")
    for g in igroups:
        for m in g.imacros:
            start = g.name + "_imacros." + m.name
            write("    if (size_t(pc - %s) < %d) return %s;\n" % (start, m.offset, start))

    write("    return NULL;\n")
    write("}\n")

if __name__ == '__main__':
    import sys
    if len(sys.argv) != 3:
        print "usage: python imacro_asm.py infile.jsasm outfile.c.out"
        sys.exit(1)

    f = open(sys.argv[2], 'w')
    try:
        assemble(sys.argv[1], f)
    finally:
        f.close()

