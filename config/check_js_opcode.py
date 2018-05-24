# vim: set ts=8 sts=4 et sw=4 tw=99:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ----------------------------------------------------------------------------
# This script checks bytecode documentation in js/src/vm/Opcodes.h
# ----------------------------------------------------------------------------

from __future__ import print_function

import os
import sys

scriptname = os.path.basename(__file__)
topsrcdir = os.path.dirname(os.path.dirname(__file__))


def log_pass(text):
    print('TEST-PASS | {} | {}'.format(scriptname, text))


def log_fail(text):
    print('TEST-UNEXPECTED-FAIL | {} | {}'.format(scriptname, text))


def check_opcode():
    sys.path.insert(0, os.path.join(topsrcdir, 'js', 'src', 'vm'))
    import opcode

    try:
        opcode.get_opcodes(topsrcdir)
    except Exception as e:
        log_fail(e.args[0])

    log_pass('ok')
    return True


def main():
    if not check_opcode():
        sys.exit(1)

    sys.exit(0)


if __name__ == '__main__':
    main()
