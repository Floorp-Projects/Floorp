// some tests...

function set_a(a) {this.a = a;}
function set_b(b) {this.b = b;}
function set_c(c) {this.c = c;}

function f_ctor(a,b,c)
{
    this.set_a = set_a;
    this.set_b = set_b;
    this.set_c = set_c;

    this.get_a = new Function("return this.a;");
    this.get_b = new Function("return this.b;");
    this.get_c = new Function("return this.c;");

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
    var b = new f2_ctor(5);
}    

A = new f2_ctor(0);
AA = new f2_ctor(100);
callMe();

A.A.set_b(8);
print(A.A.get_b());
