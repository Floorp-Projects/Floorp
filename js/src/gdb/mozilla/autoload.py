# mozilla/autoload.py: Autoload SpiderMonkey pretty-printers.

print("Loading JavaScript value pretty-printers; see js/src/gdb/README.")
print("If they cause trouble, type: disable pretty-printer .* SpiderMonkey")

import gdb.printing
import mozilla.prettyprinters

# Import the pretty-printer modules. As a side effect, loading these
# modules registers their printers with mozilla.prettyprinters.
import mozilla.GCCellPtr
import mozilla.Interpreter
import mozilla.IonGraph
import mozilla.JSObject
import mozilla.JSString
import mozilla.JSSymbol
import mozilla.Root
import mozilla.jsid
import mozilla.jsval
import mozilla.unwind

# The user may have personal pretty-printers. Get those, too, if they exist.
try:
    import my_mozilla_printers  # NOQA: F401
except ImportError:
    pass


def register(objfile):
    # Register our pretty-printers with |objfile|.
    lookup = mozilla.prettyprinters.lookup_for_objfile(objfile)
    if lookup:
        gdb.printing.register_pretty_printer(objfile, lookup, replace=True)
    mozilla.unwind.register_unwinder(objfile)
