# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import tempfile


def main(output_fd, def_file, llvm_dlltool, *llvm_dlltool_args):
    # llvm-dlltool can't output to stdout, so we create a temp file, use that
    # to write out the lib, and then copy it over to output_fd
    (tmp_fd, tmp_output) = tempfile.mkstemp()
    os.close(tmp_fd)

    try:
        cmd = [llvm_dlltool]
        cmd.extend(llvm_dlltool_args)
        cmd += ["-d", def_file, "-l", tmp_output]

        subprocess.check_call(cmd)

        with open(tmp_output, "rb") as tmplib:
            output_fd.write(tmplib.read())
    finally:
        os.remove(tmp_output)
