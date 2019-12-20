# Tests for ExecutableAllocator pretty-printing
# Ignore flake8 errors "undefined name 'assert_regexp_pretty'"
# As it caused by the way we instanciate this file
# flake8: noqa: F821

assert_subprinter_registered('SpiderMonkey', 'JS::GCCellPtr')

run_fragment('ExecutableAllocator.empty')

assert_pretty('execAlloc', 'ExecutableAllocator([])')

run_fragment('ExecutableAllocator.onepool')

reExecPool = 'ExecutablePool [a-f0-9]{8,}-[a-f0-9]{8,}'
assert_regexp_pretty('pool', reExecPool)
assert_regexp_pretty('execAlloc', 'ExecutableAllocator\(\[' + reExecPool + '\]\)')

run_fragment('ExecutableAllocator.twopools')

assert_regexp_pretty(
    'execAlloc', 'ExecutableAllocator\(\[' + reExecPool + ', ' + reExecPool + '\]\)')
