function run_test() {
  run_test_in_child("cpow_child.js", run_actual_tests);
}

function run_actual_tests() {
  var obj = getChildGlobalObject();

  test_properties(obj.data);
  test_delete(obj.data);
  test_toString(obj.data);
  test_inheritance(obj.data.derived);
  test_constructor(obj.data.constructor,
                   obj.Function,
                   obj.Object,
                   obj.Date);
  test_instanceof(obj.A, obj.B);
  test_enumeration(obj.A, obj.B);
  test_Array(obj.Array);
  test_Function(obj.Function);
  test_exceptions(obj.pitch, obj.Object);
  test_generators(obj.Function);
  test_Iterator(obj.data, obj.Iterator, obj.StopIteration);
  test_getters_setters(obj.get_set, obj.Function);
  test_forbidden_things(obj);

  do_test_finished();
}

function test_properties(data) {
  do_check_true("answer" in data);
  do_check_false("cnefhasefho" in data.nested);

  do_check_eq(data.answer, 42);
  do_check_eq(data.nested.objects.work,
              "yes they do");
  do_check_eq(data.asodfijasdiofj, void(0));

  do_check_eq(data.arr.length, 4);
  do_check_eq(data.arr[4], void(0));
  do_check_eq(data.arr[0], "zeroeth");
  do_check_eq(data.arr[1].foo, "bar");

  do_check_true(2 in data.arr);
  do_check_eq(data.arr[2], data.arr[2]); // ensure reuse
  do_check_true(data.arr[2]() === data);
  do_check_eq(data.arr[2]().arr[0], "zeroeth");
  do_check_true("call" in data.arr[2]);

  do_check_eq(data.arr[3], "last");
}

function test_delete(data) {
  do_check_eq(data.derived.try_to_delete, "just try");
  do_check_true(delete data.derived.try_to_delete);
  do_check_eq(data.derived.try_to_delete, "bwahaha");
  do_check_true(delete data.derived.try_to_delete);
  do_check_eq(data.derived.try_to_delete, "bwahaha");
}

function test_toString(data) {
  do_check_eq(data.toString(), "CPOW");
  do_check_eq(data + "asdf", "CPOWasdf");
  do_check_eq(new String(data), "CPOW");
}

function test_inheritance(derived) {
  do_check_true("inherited" in derived);
  do_check_false(derived.hasOwnProperty("inherited"));
  var base = derived.__proto__;
  do_check_true(base.hasOwnProperty("inherited"));
  do_check_eq(derived.inherited, "inherited");
  do_check_eq(derived.method(), "called");
  do_check_eq(base.method, derived.method);
  do_check_true(base.method === derived.method);
}

function test_constructor(ctor,
                          ChildFunction,
                          ChildObject,
                          ChildDate) {
  var obj = new ctor(42);
  obj.check(obj);

  // Re-run test_inheritance, creating locally everything that
  // test_inheritance expects to be created in the child process:
  var empty = new ChildFunction(),
      proto = new ChildObject();
  proto.inherited = "inherited";
  proto.method = new ChildFunction("return 'called'");
  empty.prototype = proto;
  test_inheritance(new empty);

  var cd = new ChildDate,
      tolerance_ms = 20000; // Ridiculously large to accommodate gcZeal delays.
  do_check_eq(Math.max(Math.abs(cd.getTime() - new Date),
                       tolerance_ms),
              tolerance_ms);
  do_check_true(cd instanceof ChildDate);
}

function test_enumeration(A, B) {
  function check(obj, nk, s) {
    var keys = [];
    for (var k in obj)
      keys[keys.length] = k;
    do_check_eq(keys.length, nk);
    do_check_eq(keys.sort().join("~"), s);
  }
  check(new B, 3, "a~b~c");
  check(B.prototype, 2, "a~b");
  B.prototype = A.prototype;
  A.prototype = new B;
  check(new A, 3, "a~b~c");
  check(new B, 2, "b~c");

  // Put things back the way they were, mostly:
  A.prototype = B.prototype;
  B.prototype = new A;
}

function test_instanceof(A, B) {
  var a = new A, b = new B;
  do_check_true(a instanceof A);
  do_check_false(a instanceof B);
  do_check_true(b instanceof A);
  do_check_true(b instanceof B);
}

function test_Array(ChildArray) {
  do_check_true(!!ChildArray);
  var arr = new ChildArray(1, new ChildArray(2, 3), 4);
  do_check_eq(arr.length, 3);
  do_check_eq(arr.slice(1).shift()[1], 3);
  arr[2] = arr[1];
  do_check_eq(arr.pop()[0], 2);
}

function test_Function(ChildFunction) {
  var succ = new ChildFunction("x", "return x + 1");
  do_check_eq(succ(succ(3)), 5);
  do_check_eq(succ + "", "" + new Function("x", "return x + 1"));
}

function test_exceptions(pitch, ChildObject) {
  try {
    throw "parent-only";
  } catch (x) {
    do_check_eq(x, "parent-only");
  }
  var ball = new ChildObject(),
      thrown = false;
  ball.sport = "baseball";
  try {
    pitch(ball);
    do_throw("Should have thrown.");
  } catch (x) {
    thrown = true;
    do_check_eq(x.sport, "baseball");
    do_check_eq(x, ball);
    do_check_true(x === ball);
  }
  do_check_true(thrown);
}

function test_generators(ChildFunction) {
  // Run the test with own Function just to keep sane:
  if (ChildFunction != Function)
    test_generators(Function);

  var count = 0, sum = 0,
      genFn = new ChildFunction("for(var i = 1; i < 4; i++) yield i");

  var gen = genFn();
  do try { sum += gen.next(); }
     catch (x) { break; }
  while ((count += 1));
  do_check_eq(count, 3);
  do_check_eq(sum, 6);

  try {
    for (var n in genFn()) {
      count += 1;
      sum += n;
    }
  } catch (x) {}
  do_check_eq(count, 6);
  do_check_eq(sum, 12);
}

function test_Iterator(data, ChildIterator, ChildStopIteration) {
  do_check_true(data && ChildIterator && true);
  var copy = {},
      thrown = null;
  try {
    for (var kv in ChildIterator(data)) {
      do_check_true(kv[0] in data);
      do_check_eq(data[kv[0]], kv[1]);
      copy[kv[0]] = kv[1];
    }
    // XXX I shouldn't have to be catching this, should I?
  } catch (x) { thrown = x; }
  do_check_true(thrown != null);
  do_check_true(thrown instanceof ChildStopIteration);
  do_check_eq(copy + "", "CPOW");
}

function test_getters_setters(get_set, ChildFunction) {
  do_check_eq(get_set.foo, 42);
  var thrown = null;
  try { get_set.bar = get_set.foo_throws; }
  catch (x) { thrown = x; }
  do_check_true(thrown != null);
  do_check_eq(thrown, "BAM");
  do_check_false("bar" in get_set);

  get_set.two = 2222;
  get_set.one = 1;
  do_check_eq(get_set.two, 2);

  var getter = new ChildFunction("return 'you got me'");
  get_set.__defineGetter__("defined_getter", getter);
  do_check_eq(get_set.defined_getter, "you got me");
  do_check_eq(get_set.__lookupGetter__("defined_getter"),
              getter);

  var setter = new ChildFunction("val", "this.side_effect = val");
  get_set.__defineSetter__("defined_setter", setter);
  get_set.side_effect = "can't touch this";
  get_set.defined_setter = "you set me";
  do_check_eq(get_set.side_effect, "you set me");
  do_check_eq(get_set.__lookupSetter__("defined_setter"),
              setter);
}

function test_forbidden_things(child) {
  var x_count = 0;

  do_check_eq(child.type(42), "number");

  try {
    child.type(function(){});
    do_throw("Should not have been able to pass a parent-created " +
             "function to the child");
  } catch (x) {
    print(x);
    x_count += 1;
  }

  try {
    child.type({});
    do_throw("Should not have been able to pass a parent-created " +
             "object to the child");
  } catch (x) {
    print(x);
    x_count += 1;
  }

  try {
    child.type.prop = {};
    do_throw("Should not have been able to set a property of a child " +
             "object to a parent-created object value");
  } catch (x) {
    print(x);
    x_count += 1;
  }

  try {
    child.type.prop = function(){};
    do_throw("Should not have been able to set a property of a child " +
             "object to a parent-created function value");
  } catch (x) {
    print(x);
    x_count += 1;
  }

  do_check_eq(x_count, 4);
}
