# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This module contains functionality for adding formatted, opaque "code" blocks
# into the IPDL ast. These code objects follow IPDL C++ ast patterns, and
# perform lowering in much the same way.

# In general it is recommended to use these for blocks of code which would
# otherwise be specified by building a hardcoded IPDL-C++ AST, as users of this
# API are often easier to read than users of the AST APIs in these cases.

import re
import math
import textwrap

from ipdl.cxx.ast import Node, Whitespace, GroupNode, VerbatimNode


# -----------------------------------------------------------------------------
# Public API.

def StmtCode(tmpl, **kwargs):
    """Perform template substitution to build opaque C++ AST nodes. See the
    module documentation for more information on the templating syntax.

    StmtCode nodes should be used where Stmt* nodes are used. They are placed
    on their own line and indented."""
    return _code(tmpl, False, kwargs)


def ExprCode(tmpl, **kwargs):
    """Perform template substitution to build opaque C++ AST nodes. See the
    module documentation for more information on the templating syntax.

    ExprCode nodes should be used where Expr* nodes are used. They are placed
    inline, and no trailing newline is added."""
    return _code(tmpl, True, kwargs)


def StmtVerbatim(text):
    """Build an opaque C++ AST node which emits input text verbatim.

    StmtVerbatim nodes should be used where Stmt* nodes are used. They are placed
    on their own line and indented."""
    return _verbatim(text, False)


def ExprVerbatim(text):
    """Build an opaque C++ AST node which emits input text verbatim.

    ExprVerbatim nodes should be used where Expr* nodes are used. They are
    placed inline, and no trailing newline is added."""
    return _verbatim(text, True)


# -----------------------------------------------------------------------------
# Implementation

def _code(tmpl, inline, context):
    # Remove common indentation, and strip the preceding newline from
    # '''-quoting, because we usually don't want it.
    if tmpl.startswith('\n'):
        tmpl = tmpl[1:]
    tmpl = textwrap.dedent(tmpl)

    # Process each line in turn, building up a list of nodes.
    nodes = []
    for idx, line in enumerate(tmpl.splitlines()):
        # Place newline tokens between lines in the input.
        if idx > 0:
            nodes.append(Whitespace.NL)

        # Don't indent the first line if `inline` is set.
        skip_indent = inline and idx == 0
        nodes.append(_line(line.rstrip(), skip_indent, idx + 1, context))

    # If we're inline, don't add the final trailing newline.
    if not inline:
        nodes.append(Whitespace.NL)
    return GroupNode(nodes)


def _verbatim(text, inline):
    # For simplicitly, _verbatim is implemented using the same logic as _code,
    # but with '$' characters escaped. This ensures we only need to worry about
    # a single, albeit complex, codepath.
    return _code(text.replace('$', '$$'), inline, {})


# Pattern used to identify substitutions.
_substPat = re.compile(
    r'''
    \$(?:
        (?P<escaped>\$)                  | # '$$' is an escaped '$'
        (?P<list>[*,])?{(?P<expr>[^}]+)} | # ${expr}, $*{expr}, or $,{expr}
        (?P<invalid>)                      # For error reporting
    )
    ''',
    re.IGNORECASE | re.VERBOSE)


def _line(raw, skip_indent, lineno, context):
    assert '\n' not in raw

    # Determine the level of indentation used for this line
    line = raw.lstrip()
    offset = int(math.ceil((len(raw) - len(line)) / 4))

    # If line starts with a directive, don't indent it.
    if line.startswith('#'):
        skip_indent = True

    column = 0
    children = []
    for match in _substPat.finditer(line):
        if match.group('invalid') is not None:
            raise ValueError("Invalid substitution on line %d" % lineno)

        # Any text from before the current entry should be written, and column
        # advanced.
        if match.start() > column:
            before = line[column:match.start()]
            children.append(VerbatimNode(before))
        column = match.end()

        # If we have an escaped group, emit a '$' node.
        if match.group('escaped') is not None:
            children.append(VerbatimNode('$'))
            continue

        # At this point we should have an expression.
        list_chr = match.group('list')
        expr = match.group('expr')
        assert expr is not None

        # Evaluate our expression in the context to get the values.
        try:
            values = eval(expr, context, {})
        except Exception as e:
            msg = "%s in substitution on line %d" % (repr(e), lineno)
            raise ValueError(msg) from e

        # If we aren't dealing with lists, wrap the result into a
        # single-element list.
        if list_chr is None:
            values = [values]

        # Check if this substitution is inline, or the entire line.
        inline = (match.span() != (0, len(line)))

        for idx, value in enumerate(values):
            # If we're using ',' as list mode, put a comma between each node.
            if idx > 0 and list_chr == ',':
                children.append(VerbatimNode(', '))

            # If our value isn't a node, turn it into one. Verbatim should be
            # inline unless indent isn't being skipped, and the match isn't
            # inline.
            if not isinstance(value, Node):
                value = _verbatim(str(value), skip_indent or inline)
            children.append(value)

        # If we were the entire line, indentation is handled by the added child
        # nodes. Do this after the above loop such that created verbatims have
        # the correct inline-ness.
        if not inline:
            skip_indent = True

    # Add any remaining text in the line.
    if len(line) > column:
        children.append(VerbatimNode(line[column:]))

    # If we have no children, just emit the empty string. This will become a
    # blank line.
    if len(children) == 0:
        return VerbatimNode('')

    # Add the initial indent if we aren't skipping it.
    if not skip_indent:
        children.insert(0, VerbatimNode('', indent=True))

    # Wrap ourselves into a group node with the correct indent offset
    return GroupNode(children, offset=offset)
