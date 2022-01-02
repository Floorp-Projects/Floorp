load(libdir + "asserts.js");

let x = 10;
let g = 4;

assertEq(eval(`
a => {}
/x/g;
`).toString(), "/x/g");
assertEq(eval(`
a => {}
/x/;
`).toString(), "/x/");
assertThrowsInstanceOf(() => eval(`
a => {} /x/g;
`), SyntaxError);

assertEq(eval(`
a => {},
/x/;
`).toString(), "/x/");
assertEq(eval(`
a => {}
,
/x/;
`).toString(), "/x/");

assertEq(eval(`
false ?
a => {} :
/x/;
`).toString(), "/x/");
assertEq(eval(`
false ?
a => {}
:
/x/;
`).toString(), "/x/");

assertEq(eval(`
a => {};
/x/;
`).toString(), "/x/");
assertEq(eval(`
a => {}
;
/x/;
`).toString(), "/x/");

assertEq(eval(`
a => 200
/x/g;
`) instanceof Function, true);
assertEq(eval(`
a => 200
/x/g;
`)(), 5);
assertEq(eval(`
a => 200 /x/g;
`)(), 5);

assertEq(eval(`
a => 1,
/x/;
`).toString(), "/x/");
assertEq(eval(`
a => 1
,
/x/;
`).toString(), "/x/");

assertEq(eval(`
false ?
a => 1 :
/x/;
`).toString(), "/x/");
assertEq(eval(`
false ?
a => 1
:
/x/;
`).toString(), "/x/");

assertEq(eval(`
a => 1;
/x/;
`).toString(), "/x/");
assertEq(eval(`
a => 1
;
/x/;
`).toString(), "/x/");
