# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from os.path import abspath

# TODO replace this script with a direct Python action invocation
from mozbuild.action.zip import main as create_zip

def main(output, input_dir):
    output.close()

    return create_zip(['-C', input_dir, abspath(output.name), '**'])
