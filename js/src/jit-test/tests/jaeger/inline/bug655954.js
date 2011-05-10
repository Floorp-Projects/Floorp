// |jit-test| error: TypeError
foo(); 
function foo() { 
    this();
}
