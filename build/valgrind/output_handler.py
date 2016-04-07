# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import logging
import re

class OutputHandler(object):
    '''
    A class for handling Valgrind output.

    Valgrind errors look like this:

    ==60741== 40 (24 direct, 16 indirect) bytes in 1 blocks are definitely lost in loss record 2,746 of 5,235
    ==60741==    at 0x4C26B43: calloc (vg_replace_malloc.c:593)
    ==60741==    by 0x63AEF65: PR_Calloc (prmem.c:443)
    ==60741==    by 0x69F236E: PORT_ZAlloc_Util (secport.c:117)
    ==60741==    by 0x69F1336: SECITEM_AllocItem_Util (secitem.c:28)
    ==60741==    by 0xA04280B: ffi_call_unix64 (in /builds/slave/m-in-l64-valgrind-000000000000/objdir/toolkit/library/libxul.so)
    ==60741==    by 0xA042443: ffi_call (ffi64.c:485)

    For each such error, this class extracts most or all of the first (error
    kind) line, plus the function name in each of the first few stack entries.
    With this data it constructs and prints a TEST-UNEXPECTED-FAIL message that
    TBPL will highlight.

    It buffers these lines from which text is extracted so that the
    TEST-UNEXPECTED-FAIL message can be printed before the full error.

    Parsing the Valgrind output isn't ideal, and it may break in the future if
    Valgrind changes the format of the messages, or introduces new error kinds.
    To protect against this, we also count how many lines containing
    "<insert_a_suppression_name_here>" are seen. Thanks to the use of
    --gen-suppressions=yes, exactly one of these lines is present per error. If
    the count of these lines doesn't match the error count found during
    parsing, then the parsing has missed one or more errors and we can fail
    appropriately.
    '''

    def __init__(self, logger):
        # The regexps in this list match all of Valgrind's errors. Note that
        # Valgrind is English-only, so we don't have to worry about
        # localization.
        self.logger = logger
        self.re_error = \
            r'==\d+== (' + \
            r'(Use of uninitialised value of size \d+)|' + \
            r'(Conditional jump or move depends on uninitialised value\(s\))|' + \
            r'(Syscall param .* contains uninitialised byte\(s\))|' + \
            r'(Syscall param .* points to (unaddressable|uninitialised) byte\(s\))|' + \
            r'((Unaddressable|Uninitialised) byte\(s\) found during client check request)|' + \
            r'(Invalid free\(\) / delete / delete\[\] / realloc\(\))|' + \
            r'(Mismatched free\(\) / delete / delete \[\])|' + \
            r'(Invalid (read|write) of size \d+)|' + \
            r'(Jump to the invalid address stated on the next line)|' + \
            r'(Source and destination overlap in .*)|' + \
            r'(.* bytes in .* blocks are .* lost)' + \
            r')'
        # Match identifer chars, plus ':' for namespaces, and '\?' in order to
        # match "???" which Valgrind sometimes produces.
        self.re_stack_entry = r'^==\d+==.*0x[A-Z0-9]+: ([A-Za-z0-9_:\?]+)'
        self.re_suppression = r' *<insert_a_suppression_name_here>'
        self.error_count = 0
        self.suppression_count = 0
        self.number_of_stack_entries_to_get = 0
        self.curr_error = None
        self.curr_location = None
        self.buffered_lines = None

    def log(self, line):
        self.logger(logging.INFO, 'valgrind-output', {'line': line}, '{line}')

    def __call__(self, line):
        if self.number_of_stack_entries_to_get == 0:
            # Look for the start of a Valgrind error.
            m = re.search(self.re_error, line)
            if m:
                self.error_count += 1
                self.number_of_stack_entries_to_get = 4
                self.curr_error = m.group(1)
                self.curr_location = ""
                self.buffered_lines = [line]
            else:
                self.log(line)

        else:
            # We've recently found a Valgrind error, and are now extracting
            # details from the first few stack entries.
            self.buffered_lines.append(line)
            m = re.match(self.re_stack_entry, line)
            if m:
                self.curr_location += m.group(1)
            else:
                self.curr_location += '?!?'

            self.number_of_stack_entries_to_get -= 1
            if self.number_of_stack_entries_to_get != 0:
                self.curr_location += ' / '
            else:
                # We've finished getting the first few stack entries. Print the
                # failure message and the buffered lines, and then reset state.
                self.logger(logging.ERROR, 'valgrind-error-msg',
                            {'error': self.curr_error,
                             'location': self.curr_location},
                             'TEST-UNEXPECTED-FAIL | valgrind-test | {error} at {location}')
                for b in self.buffered_lines:
                    self.log(b)
                self.curr_error = None
                self.curr_location = None
                self.buffered_lines = None

        if re.match(self.re_suppression, line):
            self.suppression_count += 1

