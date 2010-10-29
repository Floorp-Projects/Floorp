actual = '';
expected = 'zzz ,zzz 100,zzz 100,zzz 7777,zzz 100,zzz 100,zzz 8888,zzz 100,zzz 100,zzz /x/,zzz 100,zzz 100,';

//function f() {
for each(let a in ['', 7777, 8888, /x/]) {
    for each(e in ['', false, '']) {
        (function(e) {
            for each(let c in ['']) {
                appendToActual('zzz ' + a);
            }
	})();
        for (let aa = 100; aa < 101; ++aa) {
            a = aa;
        }
    }
}
//}

//f();


assertEq(actual, expected)
