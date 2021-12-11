/* Check we can delete arguments in the global space. */
arguments = 42;
reportCompare(delete arguments, true, "arguments defined as global");

