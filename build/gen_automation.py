# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distibuted with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import buildconfig
from mozbuild.preprocessor import Preprocessor


def main(output, input_file):
    pp = Preprocessor()
    pp.context.update(buildconfig.defines['ALLDEFINES'])

    substs = buildconfig.substs

    # Substs taken verbatim.
    substs_vars = (
        'BIN_SUFFIX',
    )
    for var in substs_vars:
        pp.context[var] = '"%s"' % substs[var]

    # Derived values.
    for key, condition in (
            ('IS_MAC', substs['OS_ARCH'] == 'Darwin'),
            ('IS_LINUX', substs['OS_ARCH'] == 'Linux'),
            ('IS_TEST_BUILD', substs.get('ENABLE_TESTS') == '1'),
            ('IS_DEBUG_BUILD', substs.get('MOZ_DEBUG') == '1'),
            ('CRASHREPORTER', substs.get('MOZ_CRASHREPORTER')),
            ('IS_ASAN', substs.get('MOZ_ASAN'))):
        if condition:
            pp.context[key] = '1'
        else:
            pp.context[key] = '0'

    pp.context.update({
        'XPC_BIN_PATH': '"%s/dist/bin"' % buildconfig.topobjdir,
        'CERTS_SRC_DIR': '"%s/build/pgo/certs"' % buildconfig.topsrcdir,
    })

    pp.out = output
    pp.do_include(input_file)


if __name__ == '__main__':
    main(*sys.agv[1:])
