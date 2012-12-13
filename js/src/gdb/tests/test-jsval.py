# Basic unit tests for jsval pretty-printer.

assert_subprinter_registered('SpiderMonkey', 'JS::Value')

run_fragment('jsval.simple')

assert_pretty('fortytwo', '$jsval(42)')
assert_pretty('negone', '$jsval(-1)')
assert_pretty('undefined', 'JSVAL_VOID')
assert_pretty('null', 'JSVAL_NULL')
assert_pretty('js_true', 'JSVAL_TRUE')
assert_pretty('js_false', 'JSVAL_FALSE')
assert_pretty('array_hole', '$jsmagic(JS_ARRAY_HOLE)')
assert_pretty('empty_string', '$jsval("")')
assert_pretty('friendly_string', '$jsval("Hello!")')
assert_pretty('global', '$jsval((JSObject *)  [object global] delegate)')
assert_pretty('onehundredthirtysevenonehundredtwentyeighths', '$jsval(1.0703125)')
