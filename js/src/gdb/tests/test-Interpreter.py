# Test printing interpreter internal data structures.

assert_subprinter_registered('SpiderMonkey', 'js::InterpreterRegs')

run_fragment('Interpreter.Regs')

assert_pretty('regs', '{ fp_ = , sp = fp_.slots() + 2, pc =  (JSOP_IFEQ) }')

run_fragment('Interpreter.AbstractFramePtr')

assert_pretty('sfidptr', 'AbstractFramePtr ((js::ScriptFrameIter::Data *) ) = {ptr_ = 913072}')
assert_pretty('ifptr', 'AbstractFramePtr ((js::InterpreterFrame *) ) = {ptr_ = 146464513}')
assert_pretty('bfptr', 'AbstractFramePtr ((js::jit::BaselineFrame *) ) = {ptr_ = 3135025122}')
assert_pretty('rfptr', 'AbstractFramePtr ((js::jit::RematerializedFrame *) ) = {ptr_ = 3669732611}')
