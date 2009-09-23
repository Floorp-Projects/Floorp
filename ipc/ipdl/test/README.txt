There are two major categories of tests, segregated into different
top-level directories under test/.

The first category (ipdl/) is IPDL-compiler tests.  These tests check
that the IPDL compiler is successfully compiling correct
specifications, and successfully rejecting erroneous specifications.

The second category (cxx/) is C++ tests of IPDL semantics.  These
tests check that async/sync/rpc semantics are implemented correctly,
ctors/dtors behave as they should, etc.
