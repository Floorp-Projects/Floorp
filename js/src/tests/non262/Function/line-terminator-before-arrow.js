assertThrowsInstanceOf(() => eval("() \n => {}"), SyntaxError);
assertThrowsInstanceOf(() => eval("a \n => {}"), SyntaxError);
assertThrowsInstanceOf(() => eval("(a) /*\n*/ => {}"), SyntaxError);
assertThrowsInstanceOf(() => eval("(a, b) \n => {}"), SyntaxError);
assertThrowsInstanceOf(() => eval("(a, b = 1) \n => {}"), SyntaxError);
assertThrowsInstanceOf(() => eval("(a, ...b) \n => {}"), SyntaxError);
assertThrowsInstanceOf(() => eval("(a, b = 1, ...c) \n => {}"), SyntaxError);

reportCompare(0, 0, "ok");
