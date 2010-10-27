// Bug 462027 comment 54.
function testInterpreterReentery8() {
    var e = <x><y/></x>;
    for (var j = 0; j < 4; ++j) { +[e]; }
}
testInterpreterReentery8();
