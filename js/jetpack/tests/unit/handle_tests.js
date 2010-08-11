function run_handle_tests() {
  test_sanity();
  test_safe_iteration();
  test_local_invalidation();
  test_long_parent_chain(100);
  test_invalid_creation();
}

function test_sanity() {
  var parent = createHandle(),
      child = parent.createHandle(),
      grandchild = child.createHandle();

  do_check_neq(child, parent);
  do_check_eq(child.parent, parent);
  do_check_eq(parent.parent, null);
  do_check_eq(grandchild.parent.parent, parent);

  do_check_true(child.isValid);
  do_check_true(parent.isValid);

  parent.invalidate();
}

function test_safe_iteration() {
  var handle = createHandle(),
      keys = [];
  handle.foo = 42;
  handle.self = handle;
  for (var k in handle)
    keys[keys.length] = k;
  do_check_eq(keys.sort().join("~"),
              "foo~self");
  handle.invalidate();
}

function test_local_invalidation() {
  var parent = createHandle(),
      child = parent.createHandle();

  dump("test_local_invalidation\n");
  
  child.invalidate();
  do_check_false(child.isValid);
  do_check_true(parent.isValid);

  child = parent.createHandle();
  do_check_true(child.isValid);

  parent.invalidate();
  parent.invalidate();
  do_check_false(child.isValid);
  do_check_false(parent.isValid);

  parent = createHandle();
  child = parent.createHandle();
  child = child.createHandle();

  var uncle = parent.createHandle(),
      sibling = child.parent.createHandle();

  do_check_eq(child.parent.parent, parent);
  do_check_true(child.parent.isValid);

  child.parent.invalidate();
  do_check_false(child.isValid);
  do_check_true(parent.isValid);

  do_check_false(sibling.isValid);
  do_check_true(uncle.isValid);

  parent.invalidate();
}

function test_long_parent_chain(len) {
  const ancestor = createHandle();
  for (var handle = ancestor, i = 0; i < len; ++i)
    handle = handle.createHandle();
  const child = handle;

  while (handle != ancestor)
    handle = handle.parent;

  do_check_true(child.isValid);
  ancestor.invalidate();
  do_check_false(child.isValid);
}

function test_invalid_creation() {
  var parent = createHandle(),
      child = parent.createHandle();

  parent.invalidate();

  do_check_eq(child.parent, null);

  var threw = false;
  try { child.createHandle(); }
  catch (x) { threw = true; }
  do_check_true(threw);
}
