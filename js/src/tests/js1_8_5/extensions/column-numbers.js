actual   = 'No Error';
expected = /column-numbers\.js:4:5/;
try {
    throw new Error("test");
}
catch(ex) {
    actual = ex.stack;
    print('Caught exception ' + ex.stack);
}
reportMatch(expected, actual, 'column number present');
