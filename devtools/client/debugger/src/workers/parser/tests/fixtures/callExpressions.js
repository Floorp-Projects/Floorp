dispatch({ d });
function evaluate(script, { frameId } = {frameId: 3}, {c} = {c: 2}) {}

a(b(c()));

a.b().c();
a.b.c.d();
