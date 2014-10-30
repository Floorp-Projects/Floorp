# Pretty-printers for SpiderMonkey strings.

import gdb
import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer, ptr_pretty_printer

try:
    chr(10000) # UPPER RIGHT PENCIL
except ValueError as exc: # yuck, we are in Python 2.x, so chr() is 8-bit
    chr = unichr # replace with teh unicodes

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# Cache information about the JSString type for this objfile.
class JSStringTypeCache(object):
    def __init__(self, cache):
        dummy = gdb.Value(0).cast(cache.JSString_ptr_t)
        self.ROPE_FLAGS = dummy['ROPE_FLAGS']
        self.ATOM_BIT = dummy['ATOM_BIT']
        self.INLINE_CHARS_BIT = dummy['INLINE_CHARS_BIT']
        self.TYPE_FLAGS_MASK = dummy['TYPE_FLAGS_MASK']
        self.LATIN1_CHARS_BIT = dummy['LATIN1_CHARS_BIT']

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

    def chars(self):
        d = self.value['d']
        length = d['u1']['length']
        flags = d['u1']['flags']
        is_rope = ((flags & self.stc.TYPE_FLAGS_MASK) == self.stc.ROPE_FLAGS)
        if is_rope:
            for c in JSStringPtr(d['s']['u2']['left'], self.cache).chars():
                yield c
            for c in JSStringPtr(d['s']['u3']['right'], self.cache).chars():
                yield c
        else:
            is_inline = (flags & self.stc.INLINE_CHARS_BIT) != 0
            is_latin1 = (flags & self.stc.LATIN1_CHARS_BIT) != 0
            if is_inline:
                if is_latin1:
                    chars = d['inlineStorageLatin1']
                else:
                    chars = d['inlineStorageTwoByte']
            else:
                if is_latin1:
                    chars = d['s']['u2']['nonInlineCharsLatin1']
                else:
                    chars = d['s']['u2']['nonInlineCharsTwoByte']
            for i in range(int(length)):
                yield chars[i]

    def to_string(self):
        s = u''
        for c in self.chars():
            s += chr(c)
        return s

@ptr_pretty_printer("JSAtom")
class JSAtomPtr(Common):
    def to_string(self):
        return self.value.cast(self.cache.JSString_ptr_t)
