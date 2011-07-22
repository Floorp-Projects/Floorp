// |jit-test| error: ReferenceError

if (x) {} else if ((evalcx('lazy'))++) {}
