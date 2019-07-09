# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import re
import textwrap
import string
from system_header_util import header_path

comment_re = re.compile(r'//[^\n]*\n|/\*.*\*/', re.S)
decl_re = re.compile(r'''^(.+)\s+        # type
                         (\w+)\s*        # name
                         (?:\((.*)\))?$  # optional param tys
                         ''', re.X | re.S)


def read_decls(filename):
    """Parse & yield C-style decls from an input file"""
    with open(filename, 'r') as fd:
        # Strip comments from the source text.
        text = comment_re.sub('', fd.read())

        # Parse individual declarations.
        raw_decls = [d.strip() for d in text.split(';') if d.strip()]
        for raw in raw_decls:
            match = decl_re.match(raw)
            if match is None:
                raise "Invalid decl: %s" % raw

            ty, name, params = match.groups()
            if params is not None:
                params = [a.strip() for a in params.split(',') if a.strip()]
            yield ty, name, params


def generate(fd, consts_path, unicodes_path, template_path, compiler):
    # Parse the template
    with open(template_path, 'r') as template_fd:
        template = string.Template(template_fd.read())

    decls = ''

    # Each constant should be saved to a temporary, and then re-assigned to a
    # constant with the correct name, allowing the value to be determined by
    # the actual definition.
    for ty, name, args in read_decls(consts_path):
        assert args is None, "parameters in const decl!"

        decls += textwrap.dedent("""
            #ifdef {name}
            constexpr {ty} _tmp_{name} = {name};
            #undef {name}
            constexpr {ty} {name} = _tmp_{name};
            #endif
            """.format(ty=ty, name=name))

    # Each unicode declaration defines a static inline function with the
    # correct types which calls the 'A' or 'W'-suffixed versions of the
    # function. Full types are required here to ensure that '0' to 'nullptr'
    # coersions are preserved.
    for ty, name, args in read_decls(unicodes_path):
        assert args is not None, "argument list required for unicode decl"

        # Parameter & argument string list
        params = ', '.join('%s a%d' % (ty, i) for i, ty in enumerate(args))
        args = ', '.join('a%d' % i for i in range(len(args)))

        decls += textwrap.dedent("""
            #ifdef {name}
            #undef {name}
            static inline {ty} WINAPI
            {name}({params})
            {{
            #ifdef UNICODE
              return {name}W({args});
            #else
              return {name}A({args});
            #endif
            }}
            #endif
            """.format(ty=ty, name=name, params=params, args=args))

    path = header_path('windows.h', compiler)

    # Write out the resulting file
    fd.write(template.substitute(header_path=path, decls=decls))
