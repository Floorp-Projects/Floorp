var o = {
    access() {
        super.foo.bar;
    }
};

// Delazify
assertThrowsInstanceOf(o.access, TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
