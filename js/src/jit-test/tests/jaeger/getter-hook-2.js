// PIC on CALLPROP invoking getter hook.

function foo(arr) {
  for (var i = 0; i < 100; i++)
    arr[i].caller(false);
}
arr = Object.create(Object.prototype);
first = Object.create({});
first.caller = bar;
second = Object.create({});
second.caller = bar;
for (var i = 0; i < 100; )
  arr[i++] = foo;
foo.caller;
function bar(x) {
  if (x)
    foo(arr);
}
bar(true);
