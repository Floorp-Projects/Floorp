let a = wrapWithProto(new Int8Array([1, 3, 5, 6, 9]), new Int8Array());

assertEq([...a].toString(), "1,3,5,6,9");
assertEq(a.every(e => e < 100), true);
assertEq(a.filter(e => e % 2 == 1).toString(), "1,3,5,9");
assertEq(a.find(e => e > 3), 5);
assertEq(a.findIndex(e => e % 2 == 0), 3);
assertEq(a.map(e => e * 10).toString(), "10,30,50,60,90");
assertEq(a.reduce((a, b) => a + b, ""), "13569");
assertEq(a.reduceRight((acc, e) => "(" + e + acc + ")", ""), "(1(3(5(6(9)))))");
assertEq(a.some(e => e % 2 == 0), true);

let s = "";
assertEq(a.forEach(e => s += e), undefined);
assertEq(s, "13569");

a.sort((a, b) => b - a);
assertEq(a.toString(), "9,6,5,3,1");

