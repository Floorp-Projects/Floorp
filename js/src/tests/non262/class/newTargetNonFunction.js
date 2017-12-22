// Make sure that we can plumb new.target, even if the results are going to
// throw.

assertThrowsInstanceOf(() => new ""(...Array()), TypeError);

assertThrowsInstanceOf(() => new ""(), TypeError);
assertThrowsInstanceOf(() => new ""(1), TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
