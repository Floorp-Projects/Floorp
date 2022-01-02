const otherGlobal = newGlobal();
for (var i=0; i<60; i++) {
    new otherGlobal.Array();
    bailout();
}
