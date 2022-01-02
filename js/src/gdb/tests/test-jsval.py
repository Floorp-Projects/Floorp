# Basic unit tests for jsval pretty-printer.
# flake8: noqa: F821

assert_subprinter_registered("SpiderMonkey", "JS::Value")

run_fragment("jsval.simple")

assert_pretty("fortytwo", "$JS::Int32Value(42)")
assert_pretty("fortytwoD", "$JS::DoubleValue(42.0)")
assert_pretty("negone", "$JS::Int32Value(-1)")
assert_pretty("undefined", "$JS::UndefinedValue()")
assert_pretty("null", "$JS::NullValue()")
assert_pretty("js_true", "$JS::BooleanValue(true)")
assert_pretty("js_false", "$JS::BooleanValue(false)")
assert_pretty("elements_hole", "$JS::MagicValue(JS_ELEMENTS_HOLE)")
assert_pretty("empty_string", '$JS::Value("")')
assert_pretty("friendly_string", '$JS::Value("Hello!")')
assert_pretty("symbol", '$JS::Value(Symbol.for("Hello!"))')
assert_pretty("bi", "$JS::BigIntValue()")
assert_pretty("global", "$JS::Value((JSObject *)  [object global])")
assert_pretty(
    "onehundredthirtysevenonehundredtwentyeighths", "$JS::DoubleValue(1.0703125)"
)
