// Since we put new.target at the end of the arguments vector, ensrue that it
// doesn't interact with the arguments object

var argsContent;

function argsWithNewTarget(foo) {
    assertEq(arguments.length, argsContent.length);
    for (let i = 0; i < arguments.length; i++)
        assertEq(arguments[i], argsContent[i]);
    let nt = new.target;

    // Assigning to the arguments object shouldn't infect new.target, either
    arguments[arguments.length] = 42;
    assertEq(new.target, nt);
}

// Test constructing invocations, with under and overflow
argsContent = [];
for (let i = 0; i < 100; i++)
    new argsWithNewTarget();

argsContent = [1];
for (let i = 0; i < 100; i++)
    new argsWithNewTarget(1);

argsContent = [1,2,3];
for (let i = 0; i < 100; i++)
    new argsWithNewTarget(1, 2, 3);

// Test spreadnew as well.
argsContent = [];
for (let i = 0; i < 100; i++)
    new argsWithNewTarget(...[]);

argsContent = [1];
for (let i = 0; i < 100; i++)
    new argsWithNewTarget(...[1]);

argsContent = [1,2,3];
for (let i = 0; i < 100; i++)
    new argsWithNewTarget(...[1,2,3]);

if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
