# Tests for GCCellPtr pretty-printing
# flake8: noqa: F821

assert_subprinter_registered("SpiderMonkey", "JS::GCCellPtr")

run_fragment("GCCellPtr.simple")

assert_pretty("nulll", "JS::GCCellPtr(nullptr)")
assert_pretty("object", "JS::GCCellPtr((JSObject*) )")
assert_pretty("string", "JS::GCCellPtr((JSString*) )")
assert_pretty("symbol", "JS::GCCellPtr((JS::Symbol*) )")
assert_pretty("bigint", "JS::GCCellPtr((JS::BigInt*) )")
assert_pretty("shape", "JS::GCCellPtr((js::Shape*) )")
assert_pretty("objectGroup", "JS::GCCellPtr((js::ObjectGroup*) )")
assert_pretty("baseShape", "JS::GCCellPtr((js::BaseShape*) )")
assert_pretty("script", "JS::GCCellPtr((js::BaseScript*) )")
assert_pretty("scope", "JS::GCCellPtr((js::Scope*) )")
assert_pretty("regExpShared", "JS::GCCellPtr((js::RegExpShared*) )")
