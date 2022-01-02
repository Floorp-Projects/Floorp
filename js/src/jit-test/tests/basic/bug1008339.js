
var count = 0;

function Parent() {
    // Scanning "this" properties here with Object.keys() solved the bug in my case
    //Object.keys(this);

    this.log('Parent ctor');
    this.meth1();
    this.log('data3 before : ' + this.data3);
    this.meth2();
    // Added properties lost in ChildA
    this.log('data3 after : ' + this.data3);
    this.log('');

    if (count++)
	assertEq(this.data3, 'z');
}
Parent.prototype.meth1 = function () {
    this.log('Parent.meth1()');
};
Parent.prototype.meth2 = function () {
    this.log('Parent.meth2()');
    // Requirement for the bug : Parent.meth2() needs to add data
    this.data4 = 'x';
};
Parent.prototype.log = function (data) {
    print(data);
}

// Intermediate constructor to instantiate children prototype without executing Parent constructor code
function ParentEmptyCtor() { }
ParentEmptyCtor.prototype = Parent.prototype;

function ChildA() {
    this.log('ChildA ctor');
    Parent.call(this);
}
ChildA.prototype = new ParentEmptyCtor();
// Using Object.create() instead solves the bug
//ChildA.prototype = Object.create(ParentEmptyCtor.prototype);
ChildA.prototype.constructor = ChildA;
ChildA.prototype.meth1 = function () {
    this.log('ChildA.meth1()');
    this.data3 = 'z';
};
ChildA.prototype.meth2 = function () {
    this.log('ChildA.meth2()');
};

function ChildB() {
    this.log('ChildB ctor');
    Parent.call(this);
}
ChildB.prototype = new ParentEmptyCtor();
//ChildB.prototype = Object.create(ParentEmptyCtor.prototype);
ChildB.prototype.constructor = ChildB;

function demo() {
    // Requirement for the bug : ChildB needs to be instantiated before ChildA
    new ChildB();
    new ChildA();
}
demo();
