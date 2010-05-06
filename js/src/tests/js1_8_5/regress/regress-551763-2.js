/* Check we can't delete a var-declared arguments in global space. */
var arguments = 42;
reportCompare(delete arguments, false, "arguments defined as global variable");
