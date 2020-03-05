# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import buildconfig


def main(output, data_file, data_symbol):
    if buildconfig.substs.get('WINE'):
        drive = 'z:'
    else:
        drive = ''
    output.write('''    AREA |.rdata|,ALIGN=4,DATA,READONLY
    EXPORT |{data_symbol}|[DATA]
|{data_symbol}|
    INCBIN {drive}{data_file}
    END
'''.format(data_file=data_file, data_symbol=data_symbol, drive=drive))
