# Test printing interpreter internal data structures.
# Ignore flake8 errors "undefined name 'assert_pretty'"
# As it caused by the way we instanciate this file
# flake8: noqa: F821

assert_subprinter_registered("SpiderMonkey", "js::InterpreterRegs")

run_fragment("Interpreter.Regs")

assert_pretty("regs", "{ fp_ = , sp = fp_.slots() + 2, pc =  (JSOp::True) }")

run_fragment("Interpreter.AbstractFramePtr")

assert_pretty(
    "ifptr", "AbstractFramePtr ((js::InterpreterFrame *) ) = {ptr_ = 146464512}"
)
assert_pretty(
    "bfptr", "AbstractFramePtr ((js::jit::BaselineFrame *) ) = {ptr_ = 3135025121}"
)
assert_pretty(
    "rfptr",
    "AbstractFramePtr ((js::jit::RematerializedFrame *) ) = {ptr_ = 3669732610}",
)
