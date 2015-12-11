var test = `

// #1
function base() { }

base.prototype = {
    test() {
        --super[1];
    }
}

var d = new base();
d.test();

// #2
class test2 {
    test() {
        super[1]++;
    }
}

var d = new test2();
d.test()

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
