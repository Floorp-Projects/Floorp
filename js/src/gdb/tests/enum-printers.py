# Test that we can find pretty-printers for enums.
# flake8: noqa: F821

import mozilla.prettyprinters


@mozilla.prettyprinters.pretty_printer("unscoped_no_storage")
class my_typedef(object):
    def __init__(self, value, cache):
        pass

    def to_string(self):
        return "unscoped_no_storage::success"


@mozilla.prettyprinters.pretty_printer("unscoped_with_storage")
class my_typedef(object):
    def __init__(self, value, cache):
        pass

    def to_string(self):
        return "unscoped_with_storage::success"


@mozilla.prettyprinters.pretty_printer("scoped_no_storage")
class my_typedef(object):
    def __init__(self, value, cache):
        pass

    def to_string(self):
        return "scoped_no_storage::success"


@mozilla.prettyprinters.pretty_printer("scoped_with_storage")
class my_typedef(object):
    def __init__(self, value, cache):
        pass

    def to_string(self):
        return "scoped_with_storage::success"


run_fragment("enum_printers.one")
assert_pretty("i1", "unscoped_no_storage::success")
assert_pretty("i2", "unscoped_with_storage::success")
assert_pretty("i3", "scoped_no_storage::success")
assert_pretty("i4", "scoped_with_storage::success")
