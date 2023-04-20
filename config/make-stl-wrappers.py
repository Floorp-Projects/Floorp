# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import string

from mozbuild.util import FileAvoidWrite

# The 'unused' arg is the output file from the file_generate action. We actually
# generate all the files in header_list


def gen_wrappers(unused, template_file, outdir, compiler, *header_list):
    template = open(template_file, "r").read()

    for header in header_list:
        with FileAvoidWrite(os.path.join(outdir, header)) as f:
            f.write(string.Template(template).substitute(HEADER=header))
