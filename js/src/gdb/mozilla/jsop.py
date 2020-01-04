# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Pretty-printers for JSOp and jsbytecode values.

import gdb
import gdb.types
import mozilla.prettyprinters
from mozilla.prettyprinters import pretty_printer, ptr_pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)


class JSOpTypeCache(object):
    # Cache information about the JSOp type for this objfile.
    def __init__(self, cache):
        self.tJSOp = gdb.lookup_type('JSOp')

        # Let self.jsop_names be an array whose i'th element is the name of
        # the i'th JSOp value.
        d = gdb.types.make_enum_dict(self.tJSOp)
        self.jsop_names = list(range(max(d.values()) + 1))
        for (k, v) in d.items():
            self.jsop_names[v] = k

    @classmethod
    def get_or_create(cls, cache):
        if not cache.mod_JSOp:
            cache.mod_JSOp = cls(cache)
        return cache.mod_JSOp


@pretty_printer('JSOp')
class JSOp(object):
    def __init__(self, value, cache):
        self.value = value
        self.cache = cache
        self.jotc = JSOpTypeCache.get_or_create(cache)

    def to_string(self):
        # JSOp's storage type is |uint8_t|, but gdb uses a signed value.
        # Manually convert it to an unsigned value before using it as an
        # index into |jsop_names|.
        # https://sourceware.org/bugzilla/show_bug.cgi?id=25325
        idx = int(self.value) & 0xff
        if idx < len(self.jotc.jsop_names):
            return self.jotc.jsop_names[idx]
        return "JSOP_UNUSED_{}".format(idx)


@ptr_pretty_printer('jsbytecode')
class JSBytecodePtr(mozilla.prettyprinters.Pointer):
    def __init__(self, value, cache):
        super(JSBytecodePtr, self).__init__(value, cache)
        self.jotc = JSOpTypeCache.get_or_create(cache)

    def to_string(self):
        try:
            opcode = str(self.value.dereference().cast(self.jotc.tJSOp))
        except Exception:
            opcode = 'bad pc'
        return '{} ({})'.format(self.value.cast(self.cache.void_ptr_t), opcode)
