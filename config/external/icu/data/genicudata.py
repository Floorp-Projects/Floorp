# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def main(output, data_file, data_symbol):
    output.write('''    AREA |.rdata|,ALIGN=4,DATA,READONLY
    EXPORT |{data_symbol}|[DATA]
|{data_symbol}|
    INCBIN {data_file}
    END
'''.format(data_file=data_file, data_symbol=data_symbol))
