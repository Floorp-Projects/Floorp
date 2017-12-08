// |reftest| skip-if(xulRuntime.shell)
// skip in the shell because 'arguments' is defined as a shell utility function

/* Check we can delete arguments in the global space. */
arguments = 42;
reportCompare(delete arguments, true, "arguments defined as global");

