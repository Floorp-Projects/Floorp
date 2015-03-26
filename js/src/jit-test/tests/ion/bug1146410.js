// |jit-test| error: TypeError
function foo() {
  var ws = new WeakSet();
  ws.add({});
  for (var i = 0; i < 10; i++)
    ws.add(WeakSet + "");
}
foo();
delete Math
