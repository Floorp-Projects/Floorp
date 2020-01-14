# Basic unit tests for JSOp pretty-printer.
# flake8: noqa: F821

assert_subprinter_registered('SpiderMonkey', 'JSOp')

run_fragment('jsop.simple')

assert_pretty('undefined', 'JSOP_UNDEFINED')
assert_pretty('debugger', 'JSOP_DEBUGGER')
assert_pretty('limit', 'JSOP_LIMIT')
