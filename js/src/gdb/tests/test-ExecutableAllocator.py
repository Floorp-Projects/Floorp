# Tests for ExecutableAllocator pretty-printing

assert_subprinter_registered('SpiderMonkey', 'JS::GCCellPtr')

run_fragment('ExecutableAllocator.empty')

assert_pretty('execAlloc', 'ExecutableAllocator([])')

run_fragment('ExecutableAllocator.onepool')

reExecPool = 'ExecutablePool [a-f0-9]{8,}-[a-f0-9]{8,}'
assert_regexp_pretty('pool', reExecPool)
assert_regexp_pretty('execAlloc', 'ExecutableAllocator\(\[' +reExecPool+ '\]\)')

run_fragment('ExecutableAllocator.twopools')

assert_regexp_pretty('execAlloc', 'ExecutableAllocator\(\[' + reExecPool + ', ' + reExecPool + '\]\)')
