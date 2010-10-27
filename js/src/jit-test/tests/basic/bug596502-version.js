/* All versions >= 1.6. */

function syntaxErrorFromXML() {
    try {
        var f = new Function('var text = <![CDATA[aaaaa bbbb]]>.toString();');
        return false;
    } catch (e if e instanceof SyntaxError) {
        return true;
    }
}

version(150);
assertEq(syntaxErrorFromXML(), true);
revertVersion();

for (vno in {150: null, 160: null, 170: null, 180: null}) {
    version(vno);
    assertEq(syntaxErrorFromXML(), false);
    revertVersion();
}

print('PASS!')
