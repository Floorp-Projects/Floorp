x = (/x/).exec();
y = wrapWithProto((new WeakMap), x);
y.length = 7;
Array.prototype.push.call(y, 1);
Array.prototype.reverse.call(y);
