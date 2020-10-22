# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Pretty-printers for JSID values.

import mozilla.prettyprinters
import mozilla.Root

from mozilla.prettyprinters import pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)


@pretty_printer('JS::PropertyKey')
class PropertyKey(object):
    # Since people don't always build with macro debugging info, I can't
    # think of any way to avoid copying these values here, short of using
    # inferior calls for every operation (which, I hear, is broken from
    # pretty-printers in some recent GDBs).
    TYPE_STRING = 0x0
    TYPE_INT = 0x1
    TYPE_VOID = 0x2
    TYPE_SYMBOL = 0x4
    TYPE_EMPTY = 0x6
    TYPE_MASK = 0x7

    def __init__(self, value, cache):
        self.value = value
        self.cache = cache
        self.concrete_type = self.value.type.strip_typedefs()

    def to_string(self):
        bits = self.value['asBits']
        tag = bits & PropertyKey.TYPE_MASK
        if tag == PropertyKey.TYPE_STRING:
            body = bits.cast(self.cache.JSString_ptr_t)
        elif tag & PropertyKey.TYPE_INT:
            body = bits >> 1
        elif tag == PropertyKey.TYPE_VOID:
            return "JSID_VOID"
        elif tag == PropertyKey.TYPE_SYMBOL:
            body = ((bits & ~PropertyKey.TYPE_MASK)
                    .cast(self.cache.JSSymbol_ptr_t))
        elif tag == PropertyKey.TYPE_EMPTY:
            return "JSID_EMPTY"
        else:
            body = "<unrecognized>"
        return '$jsid(%s)' % (body,)


@pretty_printer('JS::Rooted<long>')
def RootedPropertyKey(value, cache):
    # Hard-code the referent type pretty-printer for PropertyKey roots and
    # handles. See the comment for mozilla.Root.Common.__init__.
    return mozilla.Root.Rooted(value, cache, PropertyKey)


@pretty_printer('JS::Handle<long>')
def HandlePropertyKey(value, cache):
    return mozilla.Root.Handle(value, cache, PropertyKey)


@pretty_printer('JS::MutableHandle<long>')
def MutableHandlePropertyKey(value, cache):
    return mozilla.Root.MutableHandle(value, cache, PropertyKey)
