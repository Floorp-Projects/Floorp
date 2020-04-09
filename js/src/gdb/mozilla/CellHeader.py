# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import gdb


def get_header_ptr(value, ptr_t):
    # Return the pointer stored in a CellHeader subclass that wraps a pointer.
    assert value.type.strip_typedefs().tag.startswith('js::gc::CellHeaderWith')
    return value['header_'].cast(ptr_t)


def get_header_length_and_flags(value):
    # Return the contents of a CellHeaderWithLengthAndFlags.
    assert value.type.strip_typedefs().tag == \
        'js::gc::CellHeaderWithLengthAndFlags'
    header = value['header_']
    flags = header['header_']
    try:
        length = value['length_']
    except gdb.error:
        # If we couldn't fetch the length directly, it must be stored
        # within `flags`.
        length = flags >> 32
        flags = flags % 2**32
    return length, flags
