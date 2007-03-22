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
# The Original Code is mozilla.org code
#
# The Initial Developer of the Original Code is mozilla.org.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond: initial author
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

# A utility for compiling Python code, using features not available via
# the builtin compile.
#
# (a) It is not possible to compile the body of a Python function, without the
# function declaration. ie, 'return None' will always give a syntax error when
# passed to compile.
# (b) It is very tricky to compile code with the line-number starting at
# anything other than zero.
#
# Both of these are solved by this module, which uses the 'compiler' module
# XXX - sad side-effect is that Unicode is not correctly supported - 
# PyCF_SOURCE_IS_UTF8 is not exposed via compiler (in 2.3 at least)
# On the upside here, all 'src' params are unicode today, so expansion here
# requires no interface changes.

from compiler import parse, syntax, compile
from compiler.pycodegen import ModuleCodeGenerator
import compiler.ast
import new

def _fix_src(src):
    # windows first - \r\n -> \n, then for mac, remaining \r -> \n
    # Trailing whitespace can cause problems - make sure a final '\n' exists.
    return src.replace("\r\n", "\n").replace("\r", "\n") + "\n"

# from compiler.misc.set_filename - but we also adjust lineno attributes.
def set_filename_and_offset(filename, offset, tree):
    """Set the filename attribute to filename on every node in tree"""
    worklist = [tree]
    while worklist:
        node = worklist.pop(0)
        node.filename = filename
        if node.lineno is not None:
            node.lineno += offset
        worklist.extend(node.getChildNodes())

def parse_function(src, func_name, arg_names, defaults=[]):
    tree = parse(src, "exec")
    defaults = [compiler.ast.Const(d) for d in defaults]
    # Insert a Stmt with function object.
    try:
        decs = compiler.ast.Decorators([])
    except AttributeError:
        # 2.3 has no such concept (and different args!)
        func = compiler.ast.Function(func_name, arg_names, defaults, 0, None,
                                     tree.node)
    else:
        # 2.4 and later
        func = compiler.ast.Function(decs, func_name, arg_names, defaults, 0, None,
                                     tree.node)
    stmt = compiler.ast.Stmt((func,))
    tree.node = stmt
    syntax.check(tree)
    return tree

def compile_function(src, filename, func_name, arg_names, defaults=[],
                     # more args to come...
                     lineno=0): 
    assert filename, "filename is required"
    try:
        tree = parse_function(_fix_src(src), func_name, arg_names, defaults)
    except SyntaxError, err:
        err.lineno += lineno
        err.filename = filename
        raise SyntaxError, err

    set_filename_and_offset(filename, lineno, tree)

    gen = ModuleCodeGenerator(tree)
    return gen.getCode()

# And a 'standard' compile, but with the filename offset feature.
def compile(src, filename, mode='exec', flags=None, dont_inherit=None, lineno=0):
    if flags is not None or dont_inherit is not None or mode != 'exec':
        raise RuntimeError, "not implemented yet"
    try:
        tree = parse(_fix_src(src), mode)
    except SyntaxError, err:
        err.lineno += lineno
        err.filename = filename
        raise SyntaxError, err

    set_filename_and_offset(filename, lineno, tree)

    gen = ModuleCodeGenerator(tree)
    return gen.getCode()
