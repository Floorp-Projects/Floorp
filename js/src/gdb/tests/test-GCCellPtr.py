# Tests for GCCellPtr pretty-printing

assert_subprinter_registered('SpiderMonkey', 'JS::GCCellPtr')

run_fragment('GCCellPtr.simple')

assert_pretty('nulll', 'JS::GCCellPtr(nullptr)')
assert_pretty('object', 'JS::GCCellPtr((JSObject*) )')
assert_pretty('string', 'JS::GCCellPtr((JSString*) )')
assert_pretty('symbol', 'JS::GCCellPtr((JS::Symbol*) )')
