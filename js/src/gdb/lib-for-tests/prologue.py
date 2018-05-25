import gdb
import os
import re
import sys
import traceback

# testlibdir is set on the GDB command line, via --eval-command python testlibdir=...
sys.path[0:0] = [testlibdir]

active_fragment = None

# Run the C++ fragment named |fragment|, stopping on entry to |function|
# ('breakpoint', by default) and then select the calling frame.
def run_fragment(fragment, function='breakpoint'):
    # Arrange to stop at a reasonable place in the test program.
    bp = gdb.Breakpoint(function)
    try:
        gdb.execute("run %s" % (fragment,))
        # Check that we did indeed stop by hitting the breakpoint we set.
        assert bp.hit_count == 1
    finally:
        bp.delete()
    gdb.execute('frame 1')

    global active_fragment
    active_fragment = fragment

# Assert that |actual| is equal to |expected|; if not, complain in a helpful way.
def assert_eq(actual, expected):
    if actual != expected:
        raise AssertionError("""Unexpected result:
expected: %r
actual:   %r""" % (expected, actual))

# Assert that |expected| regex matches |actual| result; if not, complain in a helpful way.
def assert_match(actual, expected):
    if re.match(expected, actual, re.MULTILINE) == None:
        raise AssertionError("""Unexpected result:
expected pattern: %r
actual:           %r""" % (expected, actual))

# Assert that |value|'s pretty-printed form is |form|. If |value| is a
# string, then evaluate it with gdb.parse_and_eval to produce a value.
def assert_pretty(value, form):
    if isinstance(value, str):
        value = gdb.parse_and_eval(value)
    assert_eq(str(value), form)

# Assert that |value|'s pretty-printed form match the pattern |pattern|. If
# |value| is a string, then evaluate it with gdb.parse_and_eval to produce a
# value.
def assert_regexp_pretty(value, form):
    if isinstance(value, str):
        value = gdb.parse_and_eval(value)
    assert_match(str(value), form)

# Check that the list of registered pretty-printers includes one named
# |printer|, with a subprinter named |subprinter|.
def assert_subprinter_registered(printer, subprinter):
    # Match a line containing |printer| followed by a colon, and then a
    # series of more-indented lines containing |subprinter|.

    names = { 'printer': re.escape(printer), 'subprinter': re.escape(subprinter) }
    pat = r'^( +)%(printer)s *\n(\1 +.*\n)*\1 +%(subprinter)s *\n' % names
    output = gdb.execute('info pretty-printer', to_string=True)
    if not re.search(pat, output, re.MULTILINE):
        raise AssertionError("assert_subprinter_registered failed to find pretty-printer:\n"
                             "  %s:%s\n"
                             "'info pretty-printer' says:\n"
                             "%s" % (printer, subprinter, output))

enable_bigint = False
try:
    if gdb.lookup_type('JS::BigInt'):
        enable_bigint = True
except:
    pass

# Request full stack traces for Python errors.
gdb.execute('set python print-stack full')

# Tell GDB not to ask the user about the things we tell it to do.
gdb.execute('set confirm off', False)

# Some print settings that make testing easier.
gdb.execute('set print static-members off')
gdb.execute('set print address off')
gdb.execute('set print pretty off')
gdb.execute('set width 0')

try:
    # testscript is set on the GDB command line, via:
    # --eval-command python testscript=...
    execfile(testscript, globals(), locals())
except AssertionError as err:
    header = '\nAssertion traceback'
    if active_fragment:
        header += ' for ' + active_fragment
    sys.stderr.write(header + ':\n')
    (t, v, tb) = sys.exc_info()
    traceback.print_tb(tb)
    sys.stderr.write('\nTest assertion failed:\n')
    sys.stderr.write(str(err))
    sys.exit(1)
