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
        corrupt = {
            0x2f2f2f2f: 'JS_FRESH_NURSERY_PATTERN',
            0x2b2b2b2b: 'JS_SWEPT_NURSERY_PATTERN',
            0xe5e5e5e5: 'jemalloc freed memory',
        }.get(flags & 0xffffffff)
        if corrupt:
            for ch in "<CORRUPT:%s>" % corrupt:
                yield ch
            return
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

    def to_string(self, maxlen=200):
        s = u''
        invalid_chars_allowed = 2
        for c in self.chars():
            if len(s) >= maxlen:
                s += "..."
                break

            try:
                # Convert from gdb.Value to string.
                s += chr(c)
            except ValueError:
                if invalid_chars_allowed == 0:
                    s += "<TOO_MANY_INVALID_CHARS>"
                    break
                else:
                    invalid_chars_allowed -= 1
                    s += "\\x%04x" % (c & 0xffff)
        return s

@ptr_pretty_printer("JSAtom")
class JSAtomPtr(Common):
    def to_string(self):
        return self.value.cast(self.cache.JSString_ptr_t)
