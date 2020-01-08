#!/usr/bin/env python3 -B
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.


""" Usage: python make_opcode_doc.py

    This script generates SpiderMonkey bytecode documentation
    from js/src/vm/Opcodes.h.

    Output is written to stdout and should be pasted into the following
    MDN page:
    https://developer.mozilla.org/en-US/docs/SpiderMonkey/Internals/Bytecode
"""

from __future__ import print_function
import sys
import os


# Allow this script to be run from anywhere.
this_dir = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(0, this_dir)


import jsopcode
from xml.sax.saxutils import escape


try:
    import markdown
except ModuleNotFoundError as exc:
    if exc.name == 'markdown':
        # Right, most people won't have python-markdown installed. Suggest the
        # most likely path to getting this running.
        print("Failed to import markdown: " + exc.msg, file=sys.stderr)
        if os.path.exists(os.path.join(this_dir, "venv")):
            print("It looks like you previously created a virtualenv here. Try this:\n"
                  "    . venv/bin/activate",
                  file=sys.stderr)
            sys.exit(1)
        print("Try this:\n"
              "    pip install markdown\n"
              "Or, if you want to avoid installing things globally:\n"
              "    python3 -m venv venv && . venv/bin/activate && pip install markdown",
              file=sys.stderr)
        sys.exit(1)
    raise exc
except ImportError as exc:
    # Oh no! Markdown failed to load. Check for a specific known issue.
    if (exc.msg.startswith("bad magic number in 'opcode'")
        and os.path.isfile(os.path.join(this_dir, "opcode.pyc"))):
        print("Failed to import markdown due to bug 1506380.\n"
              "This is dumb--it's an old Python cache file in your directory. Try this:\n"
              "    rm " + this_dir + "/opcode.pyc\n"
              "The file is obsolete since November 2018.",
              file=sys.stderr)
        sys.exit(1)
    raise exc


SOURCE_BASE = 'http://dxr.mozilla.org/mozilla-central/source'


def format_flags(flags):
    flags = [flag for flag in flags if flag != 'JOF_BYTE']
    if len(flags) == 0:
        return ''

    flags = map(lambda x: x.replace('JOF_', ''), flags)
    return ' ({flags})'.format(flags=', '.join(flags))


def maybe_escape(value, format_str, fallback=""):
    if value:
        return format_str.format(escape(value))
    return fallback


OPCODE_FORMAT = """\
<dt id="{id}">{names}</dt>
<dd>
<table class="standard-table">
<tbody>
<tr><th>Operands</th><td>{operands}</td></tr>
<tr><th>Stack</th><td>{stack_uses}&rArr;{stack_defs}</td></tr>
</tbody>
</table>

{desc}
</dd>
"""


def print_opcode(opcode):
    names_template = '{name}{flags}'
    opcodes = [opcode] + opcode.group
    names = [names_template.format(name=escape(code.name),
                                   flags=format_flags(code.flags))
             for code in opcodes]

    print(OPCODE_FORMAT.format(
        id=opcodes[0].name,
        names='<br>'.join(names),
        operands=maybe_escape(opcode.operands, "<code>({})</code>", "none"),
        stack_uses=maybe_escape(opcode.stack_uses, "<code>{}</code> "),
        stack_defs=maybe_escape(opcode.stack_defs, " <code>{}</code>"),
        desc=markdown.markdown(opcode.desc)))


id_cache = dict()
id_count = dict()


def make_element_id(category, type=''):
    key = '{}:{}'.format(category, type)
    if key in id_cache:
        return id_cache[key]

    if type == '':
        id = category.replace(' ', '_')
    else:
        id = type.replace(' ', '_')

    if id in id_count:
        id_count[id] += 1
        id = '{}_{}'.format(id, id_count[id])
    else:
        id_count[id] = 1

    id_cache[key] = id
    return id


def print_doc(index):
    print("""<div>{{{{SpiderMonkeySidebar("Internals")}}}}</div>

<h2 id="Bytecode_Listing">Bytecode Listing</h2>

<p>This document is automatically generated from
<a href="{source_base}/js/src/vm/Opcodes.h">Opcodes.h</a> by
<a href="{source_base}/js/src/vm/make_opcode_doc.py">make_opcode_doc.py</a>.</p>
""".format(source_base=SOURCE_BASE))

    for (category_name, types) in index:
        print('<h3 id="{id}">{name}</h3>'.format(name=category_name,
                                                 id=make_element_id(category_name)))
        for (type_name, opcodes) in types:
            if type_name:
                print('<h4 id="{id}">{name}</h4>'.format(
                    name=type_name,
                    id=make_element_id(category_name, type_name)))
            print('<dl>')
            for opcode in opcodes:
                print_opcode(opcode)
            print('</dl>')


if __name__ == '__main__':
    if len(sys.argv) != 1:
        print("Usage: mach python make_opcode_doc.py",
              file=sys.stderr)
        sys.exit(1)
    js_src_vm_dir = os.path.dirname(os.path.realpath(__file__))
    root_dir = os.path.abspath(os.path.join(js_src_vm_dir, '..', '..', '..'))

    index, _ = jsopcode.get_opcodes(root_dir)
    print_doc(index)
