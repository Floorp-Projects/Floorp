// some tests...

function set_a(a) {this.a = a;}
function set_b(b) {this.b = b;}
function set_c(c) {this.c = c;}

function f_ctor(a,b,c)
{
    this.set_a = set_a;
    this.set_b = set_b;
    this.set_c = set_c;

// NOTE: these break JSD_LOWLEVEL_SOURCE in shell debugger
    this.get_a = new Function("return this.a;");
//    this.get_b = new Function("return this.b;");
//    this.get_c = new Function("return this.c;");

//    this.get_a = function() {return this.a;};
    this.get_b = function() {return this.b;};
    this.get_c = function() {return this.c;};

    this.set_a(a);
    this.set_b(b);
    this.set_c(c);
}

function f2_ctor(param)
{
    this.A = new f_ctor(1,2,3);
    this.b = new f_ctor("A","B","C");
    this.number = param;
}

function callMe()
{
    var A = new f2_ctor(1);
    debugger;
    var b = new f2_ctor(5);
}    

function foo(a,b,c,d,e,f)
{
    var g;
    var h;
    var i;
    var j;
    debugger;
}    

A = new f2_ctor(0);
AA = new f2_ctor(100);
callMe();
foo(1,2,3,4,5);

A.A.set_b(8);
print(A.A.get_b());
