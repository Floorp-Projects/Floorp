# Printing JS::Symbols.

assert_subprinter_registered('SpiderMonkey', 'ptr-to-JS::Symbol')

run_fragment('JSSymbol.simple')

assert_pretty('unique', 'Symbol()')
assert_pretty('unique_with_desc', 'Symbol("Hello!")')
assert_pretty('registry', 'Symbol.for("Hello!")')
assert_pretty('well_known', 'Symbol.iterator')
