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

for (vno in {160: null, 170: null, 180: null}) {
    print('Setting version to: ' + vno);
    version(Number(vno));
    assertEq(syntaxErrorFromXML(), false);
    revertVersion();
}

print('PASS!')
