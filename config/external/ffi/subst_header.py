# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Souce Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distibuted with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import sys
import buildconfig
from mozbuild.preprocessor import Preprocessor


def main(output, input_file):
    pp = Preprocessor()
    pp.context.update({
        'FFI_EXEC_TRAMPOLINE_TABLE': '0',
        'HAVE_LONG_DOUBLE': '0',
        'TARGET': buildconfig.substs['FFI_TARGET'],
        'VERSION': '',
    })
    pp.do_filter('substitution')
    pp.setMarker(None)
    pp.out = output
    pp.do_include(input_file)


if __name__ == '__main__':
    main(*sys.agv[1:])
