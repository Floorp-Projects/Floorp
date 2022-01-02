# Basic unit tests for JSOp pretty-printer.
# flake8: noqa: F821

assert_subprinter_registered("SpiderMonkey", "JSOp")

run_fragment("jsop.simple")

assert_pretty("undefined", "JSOp::Undefined")
assert_pretty("debugger", "JSOp::Debugger")
