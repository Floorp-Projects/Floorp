// Anonymous functions should take on the name of
// their arguments.
let foo = () => 1;
let bar = foo;

assertEq(foo.name, "foo");
assertEq(bar.name, "foo");

// Anonymous generator functions should behave the same.
let gen = function *() {};
assertEq(gen.name, "gen");

// Extra parenthesis should have no effects.
let abc = ((((function () {}))));
assertEq(abc.name, "abc");

// In multiple assignments, the inner most should become the name.
let a, b, c;
a = b = c = function () {};
assertEq(a.name, "c");
assertEq(b.name, "c");
assertEq(c.name, "c");

// In syntactic contexts outside of direct assignment no name is
// assigned.
let q = (0, function(){});
assertEq(q.name, "");

let yabba, dabba;
[yabba, dabba] = [() => 1, function(){}];
assertEq(yabba.name, "");
assertEq(dabba.name, "");

// Methods should also have a name property
let obj = {
    wubba: function() {},                   // "wubba"
    1: ()=>"lubba",                         // "1"
    dubdub: function*(){},                  // "dubdub"
    noOverride: function named (){},        // "named"
    obj2: {obj3: {funky: () => 1}},         // "funky"
    "1 + 2": () => 1,                       // "1 + 2"
    '" \\ \n \t \v \r \b \f \0': () => 1,   // "\" \n \t \v \r \x00"
    "\u9999": () => 1,                      // "\u9999"
    "\u9999 ": () => 1,                     // "\u9999 "
    "\u{999} ": () => 1,                    // "\u0999 "
    "\u{999}": () => 1,                     // "\u0999"
    "\x111234": () => 1,                    // "\x111234"
    inner: {'["\\bob2 - 6"]': () => 1},     // "[\"\\bob2 - 6\"]"
    "99 []": {"20 - 16": {rrr: () => 1}},   // "rrr"
    "'": () => 1,                           // "'"
}

assertEq(obj.wubba.name, "wubba");
assertEq(obj[1].name, "1");
assertEq(obj.dubdub.name, "dubdub");
assertEq(obj.noOverride.name, "named");
assertEq(obj.obj2.obj3.funky.name, "funky");
assertEq(obj["1 + 2"].name, "1 + 2");
assertEq(obj['" \\ \n \t \v \r \b \f \0'].name, "\" \\ \n \t \v \r \b \f \0");
assertEq(obj["\u9999"].name, "\u9999");
assertEq(obj["\u9999 "].name, "\u9999 ");
assertEq(obj["\u{999}"].name, "\u{999}");
assertEq(obj["\u{999} "].name, "\u{999} ");
assertEq(obj["\x111234"].name, "\x111234");
assertEq(obj.inner['["\\bob2 - 6"]'].name, '["\\bob2 - 6"]');
assertEq(obj["99 []"]["20 - 16"].rrr.name, "rrr");
assertEq(obj["'"].name, "'");

// Anonymous objects with methods.
assertEq(({a: () => 1}).a.name, "a");
assertEq(({"[abba]": {3: () => 1 }})["[abba]"][3].name, "3");
assertEq(({"[abba]": () => 1})["[abba]"].name, "[abba]");

// The method retains its name when assigned.
let zip = obj.wubba;
assertEq(zip.name, "wubba");

reportCompare(0, 0, 'ok');
