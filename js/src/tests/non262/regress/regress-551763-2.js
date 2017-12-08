// |reftest| skip-if(xulRuntime.shell)
// skip in the shell because 'arguments' is defined as a shell utility function

/* Check we can't delete a var-declared arguments in global space. */
var arguments = 42;
reportCompare(delete arguments, false, "arguments defined as global variable");
