# Test printing Handles.

assert_subprinter_registered('SpiderMonkey', 'instantiations-of-JS::Rooted')
assert_subprinter_registered('SpiderMonkey', 'instantiations-of-JS::Handle')
assert_subprinter_registered('SpiderMonkey', 'instantiations-of-JS::MutableHandle')
assert_subprinter_registered('SpiderMonkey', 'instantiations-of-js::BarrieredBase')

run_fragment('Root.handle')

assert_pretty('obj', '(JSObject * const)  [object global] delegate')
assert_pretty('mutableObj', '(JSObject *)  [object global] delegate')

run_fragment('Root.HeapSlot')

# This depends on implementation details of arrays, but since HeapSlot is
# not a public type, I'm not sure how to avoid doing *something* ugly.
assert_pretty('((js::NativeObject *) array.get())->elements_[0]', '$jsval("plinth")')

run_fragment('Root.barriers');

assert_pretty('prebarriered', '(JSObject *)  [object Object]');
assert_pretty('heapptr', '(JSObject *)  [object Object]');
assert_pretty('relocatable', '(JSObject *)  [object Object]');
assert_pretty('val', '$jsval((JSObject *)  [object Object])');
assert_pretty('heapValue', '$jsval((JSObject *)  [object Object])');
assert_pretty('prebarrieredValue', '$jsval((JSObject *)  [object Object])');
assert_pretty('relocatableValue', '$jsval((JSObject *)  [object Object])');
