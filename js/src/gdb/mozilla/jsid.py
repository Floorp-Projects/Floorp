# Pretty-printers for JSID values.

import gdb
import mozilla.prettyprinters

from mozilla.prettyprinters import pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

@pretty_printer('jsid')
class jsid(object):
    # Since people don't always build with macro debugging info, I can't
    # think of any way to avoid copying these values here, short of using
    # inferior calls for every operation (which, I hear, is broken from
    # pretty-printers in some recent GDBs).
    TYPE_STRING                 = 0x0
    TYPE_INT                    = 0x1
    TYPE_VOID                   = 0x2
    TYPE_OBJECT                 = 0x4
    TYPE_DEFAULT_XML_NAMESPACE  = 0x6
    TYPE_MASK                   = 0x7

    def __init__(self, value, cache):
        self.value = value
        self.cache = cache

    # SpiderMonkey has two alternative definitions of jsid: a typedef for
    # ptrdiff_t, and a struct with == and != operators defined on it.
    # Extract the bits from either one.
    def as_bits(self):
        if self.value.type.code == gdb.TYPE_CODE_STRUCT:
            return self.value['asBits']
        elif self.value.type.code == gdb.TYPE_CODE_INT:
            return self.value
        else:
            raise RuntimeError, ("definition of SpiderMonkey 'jsid' type"
                                 "neither struct nor integral type")

    def to_string(self):
        bits = self.as_bits()
        tag = bits & jsid.TYPE_MASK
        if tag == jsid.TYPE_STRING:
            body = bits.cast(self.cache.JSString_ptr_t)
        elif tag & jsid.TYPE_INT:
            body = bits >> 1
        elif tag == jsid.TYPE_VOID:
            return "JSID_VOID"
        elif tag == jsid.TYPE_OBJECT:
            body = ((bits & ~jsid.TYPE_MASK)
                    .cast(self.cache.JSObject_ptr_t))
        elif tag == jsid.TYPE_DEFAULT_XML_NAMESPACE:
            return "JS_DEFAULT_XML_NAMESPACE_ID"
        else:
            body = "<unrecognized>"
        return '$jsid(%s)' % (body,)
