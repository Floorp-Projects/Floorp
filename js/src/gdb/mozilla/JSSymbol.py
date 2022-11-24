# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# Pretty-printer for SpiderMonkey symbols.

import mozilla.prettyprinters
from mozilla.CellHeader import get_header_ptr
from mozilla.prettyprinters import ptr_pretty_printer

# Forget any printers from previous loads of this module.
mozilla.prettyprinters.clear_module_printers(__name__)

# JS::SymbolCode enumerators
PrivateNameSymbol = 0xFFFFFFFD
InSymbolRegistry = 0xFFFFFFFE
UniqueSymbol = 0xFFFFFFFF


@ptr_pretty_printer("JS::Symbol")
class JSSymbolPtr(mozilla.prettyprinters.Pointer):
    def __init__(self, value, cache):
        super(JSSymbolPtr, self).__init__(value, cache)
        self.value = value

    def to_string(self):
        code = int(self.value["code_"]) & 0xFFFFFFFF
        desc = str(get_header_ptr(self.value, self.cache.JSString_ptr_t))
        if code == InSymbolRegistry:
            return "Symbol.for({})".format(desc)
        elif code == UniqueSymbol:
            return "Symbol({})".format(desc)
        elif code == PrivateNameSymbol:
            return "#{}".format(desc)
        else:
            # Well-known symbol. Strip off the quotes added by the JSString *
            # pretty-printer.
            assert desc[0] == '"'
            assert desc[-1] == '"'
            return desc[1:-1]
