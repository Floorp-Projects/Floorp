// GETPROP PIC with multiple stubs containing getter hooks.

function foo(arr) {
  for (var i = 0; i < 100; i++)
    arr[i].caller;
}
arr = Object.create(Object.prototype);
first = Object.create({});
first.caller = null;
second = Object.create({});
second.caller = null;
for (var i = 0; i < 100; ) {
  arr[i++] = first;
  arr[i++] = foo;
  arr[i++] = second;
}
foo.caller;
foo(arr);
