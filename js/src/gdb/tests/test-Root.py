# Test printing Handles.
# Ignore flake8 errors "undefined name 'assert_pretty'"
# As it caused by the way we instanciate this file
# flake8: noqa: F821

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
assert_pretty('((js::NativeObject *) array.ptr)->elements_[0]', '$JS::Value("plinth")')

run_fragment('Root.barriers')

assert_pretty('prebarriered', '(JSObject *)  [object Object]')
assert_pretty('heapptr', '(JSObject *)  [object Object]')
assert_pretty('relocatable', '(JSObject *)  [object Object]')
assert_pretty('val', '$JS::Value((JSObject *)  [object Object])')
assert_pretty('heapValue', '$JS::Value((JSObject *)  [object Object])')
assert_pretty('prebarrieredValue', '$JS::Value((JSObject *)  [object Object])')
assert_pretty('relocatableValue', '$JS::Value((JSObject *)  [object Object])')
