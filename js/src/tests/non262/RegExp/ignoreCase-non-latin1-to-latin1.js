var BUGNUMBER = 1338779;
var summary = "Non-Latin1 to Latin1 mapping in ignoreCase.";

assertEq(/(\u039C)/.test("\xB5"), false);
assertEq(/(\u039C)+/.test("\xB5"), false);
assertEq(/(\u039C)/i.test("\xB5"), true);
assertEq(/(\u039C)+/i.test("\xB5"), true);
assertEq(/(\u039C)/u.test("\xB5"), false);
assertEq(/(\u039C)+/u.test("\xB5"), false);
assertEq(/(\u039C)/ui.test("\xB5"), true);
assertEq(/(\u039C)+/ui.test("\xB5"), true);

assertEq(/(\xB5)/.test("\u039C"), false);
assertEq(/(\xB5)+/.test("\u039C"), false);
assertEq(/(\xB5)/i.test("\u039C"), true);
assertEq(/(\xB5)+/i.test("\u039C"), true);
assertEq(/(\xB5)/u.test("\u039C"), false);
assertEq(/(\xB5)+/u.test("\u039C"), false);
assertEq(/(\xB5)/ui.test("\u039C"), true);
assertEq(/(\xB5)+/ui.test("\u039C"), true);


assertEq(/(\u0178)/.test("\xFF"), false);
assertEq(/(\u0178)+/.test("\xFF"), false);
assertEq(/(\u0178)/i.test("\xFF"), true);
assertEq(/(\u0178)+/i.test("\xFF"), true);
assertEq(/(\u0178)/u.test("\xFF"), false);
assertEq(/(\u0178)+/u.test("\xFF"), false);
assertEq(/(\u0178)/ui.test("\xFF"), true);
assertEq(/(\u0178)+/ui.test("\xFF"), true);

assertEq(/(\xFF)/.test("\u0178"), false);
assertEq(/(\xFF)+/.test("\u0178"), false);
assertEq(/(\xFF)/i.test("\u0178"), true);
assertEq(/(\xFF)+/i.test("\u0178"), true);
assertEq(/(\xFF)/u.test("\u0178"), false);
assertEq(/(\xFF)+/u.test("\u0178"), false);
assertEq(/(\xFF)/ui.test("\u0178"), true);
assertEq(/(\xFF)+/ui.test("\u0178"), true);


assertEq(/(\u017F)/.test("\x73"), false);
assertEq(/(\u017F)+/.test("\x73"), false);
assertEq(/(\u017F)/i.test("\x73"), false);
assertEq(/(\u017F)+/i.test("\x73"), false);
assertEq(/(\u017F)/u.test("\x73"), false);
assertEq(/(\u017F)+/u.test("\x73"), false);
assertEq(/(\u017F)/iu.test("\x73"), true);
assertEq(/(\u017F)+/iu.test("\x73"), true);

assertEq(/(\x73)/.test("\u017F"), false);
assertEq(/(\x73)+/.test("\u017F"), false);
assertEq(/(\x73)/i.test("\u017F"), false);
assertEq(/(\x73)+/i.test("\u017F"), false);
assertEq(/(\x73)/u.test("\u017F"), false);
assertEq(/(\x73)+/u.test("\u017F"), false);
assertEq(/(\x73)/iu.test("\u017F"), true);
assertEq(/(\x73)+/iu.test("\u017F"), true);


assertEq(/(\u1E9E)/.test("\xDF"), false);
assertEq(/(\u1E9E)+/.test("\xDF"), false);
assertEq(/(\u1E9E)/i.test("\xDF"), false);
assertEq(/(\u1E9E)+/i.test("\xDF"), false);
assertEq(/(\u1E9E)/u.test("\xDF"), false);
assertEq(/(\u1E9E)+/u.test("\xDF"), false);
assertEq(/(\u1E9E)/iu.test("\xDF"), true);
assertEq(/(\u1E9E)+/iu.test("\xDF"), true);

assertEq(/(\xDF)/.test("\u1E9E"), false);
assertEq(/(\xDF)+/.test("\u1E9E"), false);
assertEq(/(\xDF)/i.test("\u1E9E"), false);
assertEq(/(\xDF)+/i.test("\u1E9E"), false);
assertEq(/(\xDF)/u.test("\u1E9E"), false);
assertEq(/(\xDF)+/u.test("\u1E9E"), false);
assertEq(/(\xDF)/iu.test("\u1E9E"), true);
assertEq(/(\xDF)+/iu.test("\u1E9E"), true);


assertEq(/(\u212A)/.test("\x6B"), false);
assertEq(/(\u212A)+/.test("\x6B"), false);
assertEq(/(\u212A)/i.test("\x6B"), false);
assertEq(/(\u212A)+/i.test("\x6B"), false);
assertEq(/(\u212A)/u.test("\x6B"), false);
assertEq(/(\u212A)+/u.test("\x6B"), false);
assertEq(/(\u212A)/iu.test("\x6B"), true);
assertEq(/(\u212A)+/iu.test("\x6B"), true);

assertEq(/(\x6B)/.test("\u212A"), false);
assertEq(/(\x6B)+/.test("\u212A"), false);
assertEq(/(\x6B)/i.test("\u212A"), false);
assertEq(/(\x6B)+/i.test("\u212A"), false);
assertEq(/(\x6B)/u.test("\u212A"), false);
assertEq(/(\x6B)+/u.test("\u212A"), false);
assertEq(/(\x6B)/iu.test("\u212A"), true);
assertEq(/(\x6B)+/iu.test("\u212A"), true);


assertEq(/(\u212B)/.test("\xE5"), false);
assertEq(/(\u212B)+/.test("\xE5"), false);
assertEq(/(\u212B)/i.test("\xE5"), false);
assertEq(/(\u212B)+/i.test("\xE5"), false);
assertEq(/(\u212B)/u.test("\xE5"), false);
assertEq(/(\u212B)+/u.test("\xE5"), false);
assertEq(/(\u212B)/iu.test("\xE5"), true);
assertEq(/(\u212B)+/iu.test("\xE5"), true);

assertEq(/(\xE5)/.test("\u212B"), false);
assertEq(/(\xE5)+/.test("\u212B"), false);
assertEq(/(\xE5)/i.test("\u212B"), false);
assertEq(/(\xE5)+/i.test("\u212B"), false);
assertEq(/(\xE5)/u.test("\u212B"), false);
assertEq(/(\xE5)+/u.test("\u212B"), false);
assertEq(/(\xE5)/iu.test("\u212B"), true);
assertEq(/(\xE5)+/iu.test("\u212B"), true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
