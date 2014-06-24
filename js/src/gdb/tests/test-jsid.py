# Tests for jsid pretty-printing

assert_subprinter_registered('SpiderMonkey', 'jsid')

run_fragment('jsid.simple')

assert_pretty('string_id', '$jsid("moon")')
assert_pretty('int_id', '$jsid(1729)')
unique_symbol_pretty = str(gdb.parse_and_eval('unique_symbol_id')).split('@')[0]
assert_eq(unique_symbol_pretty, '$jsid(Symbol("moon"))')
assert_pretty('registry_symbol_id', '$jsid(Symbol.for("moon"))')
assert_pretty('well_known_symbol_id', '$jsid(Symbol.iterator)')
assert_pretty('void_id', 'JSID_VOID')
assert_pretty('empty_id', 'JSID_EMPTY')

run_fragment('jsid.handles')

assert_pretty('jsid_handle',         '$jsid("shovel")')
assert_pretty('mutable_jsid_handle', '$jsid("shovel")')
