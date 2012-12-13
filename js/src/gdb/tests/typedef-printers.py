# Test that we can find pretty-printers for typedef names, not just for
# struct types and templates.

import mozilla.prettyprinters

@mozilla.prettyprinters.pretty_printer('my_typedef')
class my_typedef(object):
    def __init__(self, value, cache):
        pass
    def to_string(self):
        return 'huzzah'

run_fragment('typedef_printers.one')
assert_pretty('i', 'huzzah')
