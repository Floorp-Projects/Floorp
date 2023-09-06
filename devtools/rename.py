# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Put the content of `filenames[0]` file into `output` file pointer
def main(output, *filenames):
    with open(filenames[0], "r", encoding="utf-8") as f:
        content = f.read()
        output.write(content)
