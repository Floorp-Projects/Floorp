# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import print_function
import os
import string
from mozbuild.util import FileAvoidWrite


def find_in_path(file, searchpath):
    for dir in searchpath.split(os.pathsep):
        f = os.path.join(dir, file)
        if os.path.exists(f):
            return f
    return ''


def header_path(header, compiler):
    if compiler == 'gcc':
        # we use include_next on gcc
        return header
    elif compiler == 'msvc':
        return find_in_path(header, os.environ.get('INCLUDE', ''))
    else:
        # hope someone notices this ...
        raise NotImplementedError(compiler)

# The 'unused' arg is the output file from the file_generate action. We actually
# generate all the files in header_list


def gen_wrappers(unused, outdir, compiler, template_file, *header_list):
    template = open(template_file, 'r').read()

    for header in header_list:
        path = header_path(header, compiler)
        with FileAvoidWrite(os.path.join(outdir, header)) as f:
            f.write(string.Template(template).substitute(HEADER=header,
                                                         HEADER_PATH=path))
