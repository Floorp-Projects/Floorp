load(libdir + "asm.js");

var body =
"   'use asm';\
    var i8=new global.Int8Array(buffer);\
    function g(i,j,k) {\
        i=i|0;\
        j=j|0;\
        k=k|0;\
        var a=0,b=0,c=0,d=0,e=0,f=0;\
        a=(i+j)|0;\
        b=(k+j)|0;\
        c=(i+k)|0;\
        b=(a+b)|0;\
        d=(b+c+i+j)|0;\
        e=(a+j+c)|0;\
        f=(a+i+k)|0;\
        i8[i] = f;\
        return (a+b+c+d+e+f)|0;\
    }\
    return g;";

var buf=new ArrayBuffer(4096);
var g = asmLink(asmCompile('global','foreign','buffer',body), this, null, buf);
assertEq(g(1,2,3), 46);
assertEq(new Int8Array(buf)[1], 7);

var body =
"   'use asm';\
    var i8=new global.Int8Array(buffer);\
    function g(i,j,k) {\
        i=i|0;\
        j=j|0;\
        k=k|0;\
        var a=0,b=0,c=0,d=0,e=0,f=0;\
        a=(i+j)|0;\
        b=(k+j)|0;\
        c=(i+k)|0;\
        b=(a+b)|0;\
        d=(b+c+i+j)|0;\
        e=(a+j+c)|0;\
        f=(a+i+k)|0;\
        i8[i] = e;\
        return (a+b+c+d+e+f)|0;\
    }\
    return g;";

var buf=new ArrayBuffer(4096);
var g = asmLink(asmCompile('global','foreign','buffer',body), this, null, buf);
assertEq(g(1,2,3), 46);
assertEq(new Int8Array(buf)[1], 9);

var body =
"   'use asm';\
    var i8=new global.Int8Array(buffer);\
    function g(i,j,k) {\
        i=i|0;\
        j=j|0;\
        k=k|0;\
        var a=0,b=0,c=0,d=0,e=0,f=0,g=0;\
        a=(i+j)|0;\
        b=(k+j)|0;\
        c=(i+k)|0;\
        b=(a+b)|0;\
        d=(b+c+i+j)|0;\
        e=(a+j+c)|0;\
        f=(a+i+k)|0;\
        g=(f+j+b)|0;\
        i8[i] = g;\
        return (a+b+c+d+e+f+g)|0;\
    }\
    return g;";

var buf=new ArrayBuffer(4096);
var g = asmLink(asmCompile('global','foreign','buffer',body), this, null, buf);
assertEq(g(1,2,3), 63);
assertEq(new Int8Array(buf)[1], 17);
