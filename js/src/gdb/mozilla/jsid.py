# Pretty-printers for JSID values.

import gdb
import mozilla.prettyprinters
import mozilla.Root

from mozilla.prettyprinters import pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)


@pretty_printer('jsid')
class jsid(object):
    # Since people don't always build with macro debugging info, I can't
    # think of any way to avoid copying these values here, short of using
    # inferior calls for every operation (which, I hear, is broken from
    # pretty-printers in some recent GDBs).
    TYPE_STRING = 0x0
    TYPE_INT = 0x1
    TYPE_VOID = 0x2
    TYPE_SYMBOL = 0x4
    TYPE_MASK = 0x7

    def __init__(self, value, cache):
        self.value = value
        self.cache = cache
        self.concrete_type = self.value.type.strip_typedefs()

    # SpiderMonkey has two alternative definitions of jsid: a typedef for
    # ptrdiff_t, and a struct with == and != operators defined on it.
    # Extract the bits from either one.
    def as_bits(self):
        if self.concrete_type.code == gdb.TYPE_CODE_STRUCT:
            return self.value['asBits']
        elif self.concrete_type.code == gdb.TYPE_CODE_INT:
            return self.value
        else:
            raise RuntimeError("definition of SpiderMonkey 'jsid' type"
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
        elif tag == jsid.TYPE_SYMBOL:
            if bits == jsid.TYPE_SYMBOL:
                return "JSID_EMPTY"
            body = ((bits & ~jsid.TYPE_MASK)
                    .cast(self.cache.JSSymbol_ptr_t))
        else:
            body = "<unrecognized>"
        return '$jsid(%s)' % (body,)


@pretty_printer('JS::Rooted<long>')
def RootedJSID(value, cache):
    # Hard-code the referent type pretty-printer for jsid roots and handles.
    # See the comment for mozilla.Root.Common.__init__.
    return mozilla.Root.Rooted(value, cache, jsid)


@pretty_printer('JS::Handle<long>')
def HandleJSID(value, cache):
    return mozilla.Root.Handle(value, cache, jsid)


@pretty_printer('JS::MutableHandle<long>')
def MutableHandleJSID(value, cache):
    return mozilla.Root.MutableHandle(value, cache, jsid)
