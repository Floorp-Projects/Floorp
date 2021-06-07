.. _using_compiler_explorer:

Using Compiler Explorer
=======================

As noted in the previous section, :ref:`using_clang_query`,
it can be useful to prototype
and develop static analysis with live results. For this reason, we are running a
`custom instance of the Godbolt's Compiler Explorer <https://foxyeah.com/>`_
at foxyeah.com (currently behind SSO).
Our custom instance is bundled with our header files from mozilla-central,
enabled by default.

When first started, Compiler Explorer will provide you with example source code and it's
its compilation results in assembly - when running a specific toolchain.
Let's first ensure this matches our default toolchain for Firefox:
1. Click the dropdown that (currently) defaults to x86-64 gcc and switch it to our current version of x86-64 clang.
2. Once you have selected the correct compiler, find the tool button below and enable clang-query.
3. Click the *Stdin* button to be able to supply arguments to clang-query
4. Fill-in the Recommended Boilerplate from our previous section

::

  set traversal     IgnoreUnlessSpelledInSource
  set bind-root     true # Unless you use any .bind("foo") commands
  set print-matcher true
  enable output     dump


Now, you should be able to match a wide variety of C++ source code expressions.
Start with something simple like `match callExpr()` and narrow it down from here.
Continue at the next section, :ref:`writing_matchers`.