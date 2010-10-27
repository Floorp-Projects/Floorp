actual = '';
expected = 'bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,bundefined,aundefined,b[object Object],aundefined,';

// tests nfixed case of getting slot with let.

for (var q = 0; q < 10; ++q) {
  for each(let b in [(void 0), {}]) {
        appendToActual('a' + ((function() {
            for (var e in ['']) {
                appendToActual('b' + b)
            }
        })()))
    }
}


assertEq(actual, expected)
