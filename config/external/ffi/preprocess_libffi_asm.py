# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Souce Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distibuted with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import print_function
import buildconfig
import mozpack.path as mozpath
import os
import shlex
import subprocess


def main(output, input_asm, ffi_h, ffi_config_h, defines, includes):
    defines = shlex.split(defines)
    includes = shlex.split(includes)
    # CPP uses -E which generates #line directives. -EP suppresses them.
    # -TC forces the compiler to treat the input as C.
    cpp = buildconfig.substs['CPP'] + ['-EP'] + ['-TC']
    input_asm = mozpath.relpath(input_asm, os.getcwd())
    args = cpp + defines + includes + [input_asm]
    print(' '.join(args))
    preprocessed = subprocess.check_output(args)
    output.write(preprocessed)
