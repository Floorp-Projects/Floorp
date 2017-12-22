function toSource(o) { return o === null ? "null" : o.toSource(); }

var gcgcz = /((?:.)+)((?:.)*)/; /* Greedy capture, greedy capture zero. */
reportCompare(["a", "a", ""].toSource(), gcgcz.exec("a").toSource());
reportCompare(["ab", "ab", ""].toSource(), gcgcz.exec("ab").toSource());
reportCompare(["abc", "abc", ""].toSource(), gcgcz.exec("abc").toSource());

reportCompare(["a", ""].toSource(), toSource(/((?:)*?)a/.exec("a")));
reportCompare(["a", ""].toSource(), toSource(/((?:.)*?)a/.exec("a")));
reportCompare(["a", ""].toSource(), toSource(/a((?:.)*)/.exec("a")));

reportCompare(["B", "B"].toSource(), toSource(/([A-Z])/.exec("fooBar")));

// These just mustn't crash. See bug 872971
try { reportCompare(/x{2147483648}x/.test('1'), false); } catch (e) {}
try { reportCompare(/x{2147483648,}x/.test('1'), false); } catch (e) {}
try { reportCompare(/x{2147483647,2147483648}x/.test('1'), false); } catch (e) {}
// Same for these. See bug 813366
try { reportCompare("".match(/.{2147483647}11/), null); } catch (e) {}
try { reportCompare("".match(/(?:(?=g)).{2147483648,}/ + ""), null); } catch (e) {}
