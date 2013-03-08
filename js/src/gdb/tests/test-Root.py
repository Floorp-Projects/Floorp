# Test printing Handles.

assert_subprinter_registered('SpiderMonkey', 'instantiations-of-JS::Rooted')
assert_subprinter_registered('SpiderMonkey', 'instantiations-of-JS::Handle')
assert_subprinter_registered('SpiderMonkey', 'instantiations-of-JS::MutableHandle')
assert_subprinter_registered('SpiderMonkey', 'instantiations-of-js::EncapsulatedPtr')
assert_subprinter_registered('SpiderMonkey', 'js::EncapsulatedValue')

run_fragment('Root.handle')

assert_pretty('obj', '(JSObject * const)  [object global] delegate')
assert_pretty('mutableObj', '(JSObject *)  [object global] delegate')

run_fragment('Root.HeapSlot')

# This depends on implementation details of arrays, but since HeapSlot is
# not a public type, I'm not sure how to avoid doing *something* ugly.
assert_pretty('array->elements[0]', '$jsval("plinth")')

