function Integer( value, exception ) { }
try {
new Integer( Math.LN2, ++INVALID_INTEGER_VALUE? exception+1.1: 1900 );
} catch (e) {}
