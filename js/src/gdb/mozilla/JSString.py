# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Pretty-printers for SpiderMonkey strings.

import gdb
import mozilla.prettyprinters
from mozilla.prettyprinters import ptr_pretty_printer
from mozilla.CellHeader import get_header_length_and_flags

try:
    chr(10000)  # UPPER RIGHT PENCIL
except ValueError:  # yuck, we are in Python 2.x, so chr() is 8-bit
    chr = unichr  # replace with teh unicodes

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)


class JSStringTypeCache(object):
    # Cache information about the JSString type for this objfile.
    def __init__(self, cache):
        dummy = gdb.Value(0).cast(cache.JSString_ptr_t)
        self.ATOM_BIT = dummy["ATOM_BIT"]
        self.LINEAR_BIT = dummy["LINEAR_BIT"]
        self.INLINE_CHARS_BIT = dummy["INLINE_CHARS_BIT"]
        self.TYPE_FLAGS_MASK = dummy["TYPE_FLAGS_MASK"]
        self.LATIN1_CHARS_BIT = dummy["LATIN1_CHARS_BIT"]


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
        d = self.value["d"]
        length, flags = get_header_length_and_flags(self.value, self.cache)

        corrupt = {
            0x2F2F2F2F: "JS_FRESH_NURSERY_PATTERN",
            0x2B2B2B2B: "JS_SWEPT_NURSERY_PATTERN",
            0xE5E5E5E5: "jemalloc freed memory",
        }.get(flags & 0xFFFFFFFF)
        if corrupt:
            for ch in "<CORRUPT:%s>" % corrupt:
                yield ch
            return
        is_rope = (flags & self.stc.LINEAR_BIT) == 0
        if is_rope:
            for c in JSStringPtr(d["s"]["u2"]["left"], self.cache).chars():
                yield c
            for c in JSStringPtr(d["s"]["u3"]["right"], self.cache).chars():
                yield c
        else:
            is_inline = (flags & self.stc.INLINE_CHARS_BIT) != 0
            is_latin1 = (flags & self.stc.LATIN1_CHARS_BIT) != 0
            if is_inline:
                if is_latin1:
                    chars = d["inlineStorageLatin1"]
                else:
                    chars = d["inlineStorageTwoByte"]
            else:
                if is_latin1:
                    chars = d["s"]["u2"]["nonInlineCharsLatin1"]
                else:
                    chars = d["s"]["u2"]["nonInlineCharsTwoByte"]
            for i in range(int(length)):
                yield chars[i]

    def to_string(self, maxlen=200):
        s = ""
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
                    s += "\\x%04x" % (c & 0xFFFF)
        return s


@ptr_pretty_printer("JSAtom")
class JSAtomPtr(Common):
    def to_string(self):
        return self.value.cast(self.cache.JSString_ptr_t)
