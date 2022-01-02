# Basic unit tests for jsbytecode* pretty-printer.
# flake8: noqa: F821

assert_subprinter_registered("SpiderMonkey", "ptr-to-jsbytecode")

run_fragment("jsbytecode.simple")

assert_pretty("ok", "true")
assert_pretty("code", " (JSOp::Debugger)")
