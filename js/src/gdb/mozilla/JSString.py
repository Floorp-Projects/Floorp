# Pretty-printers for SpiderMonkey strings.

import gdb
import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer, ptr_pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# Cache information about the JSString type for this objfile.
class JSStringTypeCache(object):
    def __init__(self, cache):
        dummy = gdb.Value(0).cast(cache.JSString_ptr_t)
        self.LENGTH_SHIFT = dummy['LENGTH_SHIFT']
        self.FLAGS_MASK   = dummy['FLAGS_MASK']
        self.ROPE_FLAGS   = dummy['ROPE_FLAGS']
        self.ATOM_BIT     = dummy['ATOM_BIT']

class Common(mozilla.prettyprinters.Pointer):
    def __init__(self, value, cache):
        super(Common, self).__init__(value, cache)
        if not cache.mod_JSString:
            cache.mod_JSString = JSStringTypeCache(cache)
        self.stc = cache.mod_JSString

@ptr_pretty_printer("JSString")
class JSStringPtr(Common):
    def display_hint(self):
        return "string"

    def jschars(self):
        d = self.value['d']
        lengthAndFlags = d['lengthAndFlags']
        length = lengthAndFlags >> self.stc.LENGTH_SHIFT
        is_rope = (lengthAndFlags & self.stc.FLAGS_MASK) == self.stc.ROPE_FLAGS
        if is_rope:
            for c in JSStringPtr(d['u1']['left'], self.cache).jschars():
                yield c
            for c in JSStringPtr(d['s']['u2']['right'], self.cache).jschars():
                yield c
        else:
            chars = d['u1']['chars']
            for i in xrange(length):
                yield chars[i]

    def to_string(self):
        s = u''
        for c in self.jschars():
            s += unichr(c)
        return s

@ptr_pretty_printer("JSAtom")
class JSAtomPtr(Common):
    def to_string(self):
        return self.value.cast(self.cache.JSString_ptr_t)
