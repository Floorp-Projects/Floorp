var gcgcz = /((?:.)+)((?:.)*)/; /* Greedy capture, greedy capture zero. */
assertEqArray(["a", "a", ""], gcgcz.exec("a"));
assertEqArray(["ab", "ab", ""], gcgcz.exec("ab"));
assertEqArray(["abc", "abc", ""], gcgcz.exec("abc"));

assertEqArray(["a", ""], /((?:)*?)a/.exec("a"));
assertEqArray(["a", ""], /((?:.)*?)a/.exec("a"));
assertEqArray(["a", ""], /a((?:.)*)/.exec("a"));

assertEqArray(["B", "B"], /([A-Z])/.exec("fooBar"));

// These just mustn't crash. See bug 872971
try { reportCompare(/x{2147483648}x/.test('1'), false); } catch (e) {}
try { reportCompare(/x{2147483648,}x/.test('1'), false); } catch (e) {}
try { reportCompare(/x{2147483647,2147483648}x/.test('1'), false); } catch (e) {}
// Same for these. See bug 813366
try { reportCompare("".match(/.{2147483647}11/), null); } catch (e) {}
try { reportCompare("".match(/(?:(?=g)).{2147483648,}/ + ""), null); } catch (e) {}
