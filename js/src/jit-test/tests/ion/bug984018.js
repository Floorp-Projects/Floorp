var x = 0;
function test() {
    function sincos1(a, b) {
        var sa = Math.sin(a);
        var sb = Math.sin(b);
        var ca = Math.cos(a);
        var cb = Math.cos(b);
        var ra = sa * sa + ca * ca;
        var rb = sb * sb + cb * cb;
        var dec = 100000;

        assertEq(Math.round(ra * dec) / dec, Math.round(rb * dec) / dec);

        ca = Math.cos(a);
        cb = Math.cos(b);
        sa = Math.sin(a);
        sb = Math.sin(b);

        var ra = sa * sa + ca * ca;
        var rb = sb * sb + cb * cb;

        assertEq(Math.round(ra * dec) / dec, Math.round(rb * dec) / dec);
        return ra;
    }

    function sincos2(x) {
        var s1 = Math.sin(x);
        var c1 = Math.cos(x);

        var c2 = Math.cos(x);
        var s2 = Math.sin(x);
        
        return (s1 * s1 + c1 * c1) - (s2 * s2 + c2 * c2); 
    }

    function bailoutHere() { bailout(); }

    function sincos3(x) {
        var s = Math.sin(x);
        bailoutHere();
        var c = Math.cos(x);
        assertEq(Math.round(s * s + c * c), 1);
        return s;
    }

    function sincos4(x) {
        if (x < 2500)
            x = Math.sin(x);
        else
            x = Math.cos(x);
        return x;
    }

    for (var i=0; i<5000; i++) {
        x += sincos1(x, x + 1);
        x += sincos2(x);
        x += sincos3(x);
        x += sincos4(i);
    }
}
x += 1;
test();
