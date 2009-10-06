To add a new IPDL C++ unit test, you need to create (at least) the
following files (for a test "TestFoo"):

  - PTestFoo.ipdl, specifying the top-level protocol used for the test

  - TestFoo.h, declaring the top-level parent/child actors used for
    the test

  - TestFoo.cpp, defining the top-level actors

Next

  - add PTestFoo.ipdl to ipdl.mk

  - append TestFoo to the variable IPDLTESTS in Makefile.in

The IPDL test harness will try to execute |testFooParentActor->Main()|
to kick off your test.  Make sure you define |TestFooParent::Main()|.

If your test passes its criteria, please call
|MOZ_IPDL_TESTPASS("msg")| and "exit gracefully".

If your tests fails, please call |MOZ_IPDL_TESTFAIL("msg")| and "exit
ungracefully", preferably by abort()ing.


If all goes well, running

  make -C $OBJDIR/ipc/ipdl/test/cxx

will update the file IPDLUnitTests.cpp (the test launcher), and your
new code will be built automatically.


You can launch your new test by invoking

  make -C $OBJDIR/ipc/ipdl/test/cxx check

If you want to launch only your test, run

  cd $OBJDIR/dist/bin
  ./run-mozilla.sh ./ipdlunittest TestFoo


For a bare-bones example of adding a test, take a look at
PTestSanity.ipdl, TestSanity.h, TestSanity.cpp, and how "TestSanity"
is included in ipdl.mk and Makefile.in.
