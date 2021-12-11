To add a new IPDL C++ unit test, you need to create (at least) the
following files (for a test "TestFoo"):

  - PTestFoo.ipdl, specifying the top-level protocol used for the test

  - TestFoo.h, declaring the top-level parent/child actors used for
    the test

  - TestFoo.cpp, defining the top-level actors

  - (make sure all are in the namespace mozilla::_ipdltest)

Next

  - add PTestFoo.ipdl to ipdl.mk

  - append TestFoo to the variable IPDLTESTS in Makefile.in

You must define three methods in your |TestFooParent| class:

  - static methods |bool RunTestInProcesses()| and
    |bool RunTestInThreads()|.  These methods control whether
    to execute the test using actors in separate processes and
    threads respectively.  Generally, both should return true.

  - an instance method |void Main()|.  The test harness wil first
    initialize the processes or threads, create and open both actors,
    and then kick off the test using |Main()|.  Make sure you define
    it.

If your test passes its criteria, please call
|MOZ_IPDL_TESTPASS("msg")| and "exit gracefully".

If your tests fails, please call |MOZ_IPDL_TESTFAIL("msg")| and "exit
ungracefully", preferably by abort()ing.


If all goes well, running

  make -C $OBJDIR/ipc/ipdl/test/cxx

will update the file IPDLUnitTests.cpp (the test launcher), and your
new code will be built automatically.


You can launch your new test by invoking one of

  make -C $OBJDIR/ipc/ipdl/test/cxx check-proc     (test process-based tests)
  make -C $OBJDIR/ipc/ipdl/test/cxx check-threads  (test thread-based tests)
  make -C $OBJDIR/ipc/ipdl/test/cxx check          (tests both)

If you want to launch only your test, run

  cd $OBJDIR/dist/bin
  ./run-mozilla.sh ./ipdlunittest TestFoo          (test in two processes, if appl.)
  ./run-mozilla.sh ./ipdlunittest thread:TestFoo   (test in two threads, if appl.)


For a bare-bones example of adding a test, take a look at
PTestSanity.ipdl, TestSanity.h, TestSanity.cpp, and how "TestSanity"
is included in ipdl.mk and Makefile.in.
