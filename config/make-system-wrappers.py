# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import print_function
import os
from mozbuild.util import FileAvoidWrite

header_template = '''#pragma GCC system_header
#pragma GCC visibility push(default)
#include_next <%(header)s>
#pragma GCC visibility pop
'''


# The 'unused' arg is the output file from the file_generate action. We actually
# generate all the files in header_list
def gen_wrappers(unused, outdir, *header_list):
    for header in header_list:
        with FileAvoidWrite(os.path.join(outdir, header)) as f:
            f.write(header_template % {
                'header': header,
            })
