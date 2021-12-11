# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distibuted with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
from mozbuild.preprocessor import Preprocessor


def main(output, input_file, version):
    pp = Preprocessor()
    pp.context.update(
        {
            "VERSION": version,
        }
    )
    pp.out = output
    pp.do_include(input_file)


if __name__ == "__main__":
    main(*sys.agv[1:])
