Last modified 16 July 1998

<<< Under construction >>>

Unix platforms are probably the easiest platforms to port NetScape
Portable Runtime (NSPR) to. However, if you are not familiar with the
structure of the NSPR source tree and its build system, it may take you
extra time. Therefore I write this article to document the more
mechanical part of the Unix porting task. For certain more "standard"
Unix platforms, this may be all you have to do. On other platforms, you
may need to do extra work to deal with their idiosyncrasies.

.. _Porting_Instructions:

Porting Instructions
--------------------

You can use different threading packages to implement NSPR threads. NSPR
has a user-level threading library where thread context switches are
done by <tt>setjmp/longjmp</tt> or <tt>sigsetjmp/siglongjmp</tt>. This
is called the *local threads only* version of *classic NSPR*. You should
attempt to do a local threads only port first. The classic NSPR source
files are in <tt>mozilla/nsprpub/pr/src/threads</tt> and
<tt>mozilla/nsprpub/pr/src/io</tt>.

If the platform has pthreads, you can also use pthreads as an
implementation strategy. This is referred to as *pthreads NSPR*.
Pthreads NSPR has relatively orthogonal source code in the thread
management, thread synchronization, and I/O area. The pthreads source
files are in <tt>mozilla/nsprpub/pr/src/pthreads</tt>.

I use the NetBSD port I recently received as an example to illustrate
the mechanical part of the porting process.

There are a few new files you need to add:

 <tt>mozilla/nsprpub/config/NetBSD.mk</tt> 
   The name of this file is the return value of <tt>uname -s</tt> on the
   platform, plus the file suffix <tt>.mk</tt>. If the return value of
   <tt>uname -s</tt> is too long or ambiguous, you can modify it in
   <tt>mozilla/nsprpub/config/arch.mk</tt> (the makefile variable
   <tt>OS_ARCH</tt>).
 <tt>mozilla/nsprpub/pr/include/md/_netbsd.cfg</tt> 
   We have a program <tt>mozilla/nsprpub/pr/include/gencfg.c</tt> to
   help you generate *part*\ of this file. You can build the
   <tt>gencfg</tt> tool as follows:

.. code:: eval

   cd mozilla/nsprpub/pr/include
   gmake gencfg
   gencfg > foo.bar

Then you use the macro values in <tt>foo.bar</tt> as a basis for the
<tt>_xxxos.cfg</tt> file.

-  <tt>mozilla/nsprpub/pr/include/md/_netbsd.h</tt>: For local threads
   only version, the main challenge is to figure out how to define the
   three thread context switch macros. In particular, you need to figure
   out which element in the <tt>jmp_buf</tt> is the stack pointer.
   Usually <tt>jmp_buf</tt> is an array of integers, and some platforms
   have a <tt>JB_SP</tt> macro that tells you which array element is the
   stack pointer. If you can't find a <tt>JB_SP</tt> macro, you must
   resort to brute-force experiments. I usually print out every element
   in the <tt>jmp_buf</tt> and see which one is the closest to the
   address of a local variable (local variables are allocated on the
   stack). On some platforms, <tt>jmp_buf</tt> is a struct, then you
   should look for a struct member named <tt>sp</tt> or something
   similar.

-  <tt>mozilla/nsprpub/pr/src/md/unix/netbsd.c</tt>

You need to modify the following existing files:

-  <tt>mozilla/nsprpub/pr/include/md/Makefile</tt>
-  <tt>mozilla/nsprpub/pr/include/md/_unixos.h</tt>: Just fix the
   compiling errors, usually pointed out by the <tt>#error</tt>
   preprocessor directives we inserted.
-  <tt>mozilla/nsprpub/pr/include/md/prosdep.h</tt>
-  <tt>mozilla/nsprpub/pr/src/md/prosdep.c</tt>
-  <tt>mozilla/nsprpub/pr/src/md/unix/Makefile</tt>
-  <tt>mozilla/nsprpub/pr/src/md/unix/objs.mk</tt>
-  <tt>mozilla/nsprpub/pr/src/md/unix/unix.c</tt>: Just fix the
   compiling errors, usually pointed out by the <tt>#error</tt>
   preprocessor directives we inserted

For a pthreads port, you need to modify the following files:

-  <tt>mozilla/nsprpub/pr/include/md/_pth.h</tt>
-  Files in mozilla/nsprpub/pr/src/pthreads, most likely
   <tt>ptthread.c</tt> and <tt>ptio.c</tt>

.. _Testing_Your_Port:

Testing Your Port
-----------------

We have some unit tests in <tt>mozilla/nsprpub/pr/tests</tt>. First a
warning: some of the tests are broken. Further, we don't have
documentation of our unit tests, so you often need to resort to read the
source code to understand what they do. Some of them respond to the
<tt>-h</tt> command line option and print a usage message. <strike>Henry
Sobotka of the OS/2 Mozilla porting project has a web page at
`http://www.axess.com/users/sobotka/n.../warpztjs.html <http://www.axess.com/users/sobotka/nsprtest/warpztjs.html>`__
with good documentation of the NSPR unit tests</strike>.

Here are my favorite tests.

For thread management and synchronization:

-  <tt>cvar -d</tt>
-  <tt>cvar2</tt>
-  <tt>./join -d</tt>
-  <tt>perf</tt>
-  <tt>./switch -d</tt>
-  <tt>intrupt -d</tt>

For I/O:

-  <tt>cltsrv -d</tt>, <tt>cltsrv -Gd</tt>
-  <tt>socket</tt>
-  <tt>testfile -d</tt>
-  <tt>tmocon -d</tt>
-  '<tt>tmoacc -d</tt>' in conjunction with '<tt>writev -d</tt>'

Miscellaneous:

-  <tt>dlltest -d</tt>
-  <tt>forktest</tt>

.. _Original_Document_Information:

Original Document Information
-----------------------------

-  Author: larryh@netscape.com
-  Last Updated Date: 16 July 1998
