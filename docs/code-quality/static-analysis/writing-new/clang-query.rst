.. _using_clang_query:

Using clang-query
=================

clang-query is a tool that allows you to quickly iterate and develop the difficult part of a matcher.
Once the design of the matcher is completed, it can be transferred to a C++ clang-tidy plugin, `similar
to the ones in mozilla-central <https://searchfox.org/mozilla-central/source/build/clang-plugin>`_.

Recommended Boilerplate
-----------------------

::

  set traversal     IgnoreUnlessSpelledInSource
  set bind-root     true # Unless you use any .bind("foo") commands
  set print-matcher true
  enable output     dump


clang-query Options
-------------------

set traversal
~~~~~~~~~~~~~

`Traversal mode <https://clang.llvm.org/docs/LibASTMatchersReference.html#traverse-mode>`_ specifies how the AST Matcher will traverse the nodes in the Abstract Syntax Tree.  There are two values:

AsIs
  This mode notes all the nodes in the AST, even if they are not explicitly spelled out in the source. This will include nodes you have never seen and probably don't immediately understand, for example ``ExprWithCleanups`` and ``MaterializeTemporaryExpr``. In this mode, it is necessary to write matchers that expliticly match or otherwise traverse these potentially unexpected nodes.

IgnoreUnlessSpelledInSource
  This mode skips over 'implicit' nodes that are created as a result of implicit casts or other usually-low-level language details. This is typically much more user-friendly. **Typically you would want to use**  ``set traversal IgnoreUnlessSpelledInSource``.

More examples are available `in the documentation <https://clang.llvm.org/docs/LibASTMatchersReference.html#traverse-mode>`_, but here is a simple example:

::

  B func1() { 
    return 42; 
  }
  
  /* 
    AST Dump in 'Asis' mode for C++17/C++20 dialect:
  
    FunctionDecl
    `-CompoundStmt
      `-ReturnStmt
        `-ImplicitCastExpr
          `-CXXConstructExpr
            `-IntegerLiteral 'int' 42
    
    AST Dump in 'IgnoreUnlessSpelledInSource' mode for all dialects:

    FunctionDecl
    `-CompoundStmt
      `-ReturnStmt
        `-IntegerLiteral 'int' 42
  */


set bind-root
~~~~~~~~~~~~~

If you are matching objects and assigning them names for later use, this option may be relevant.  If you are debugging a single matcher and not using any ``.bind()``, it is irrelevant.

Consider the output of ``match functionDecl().bind("x")``:

::

  clang-query> match functionDecl().bind("x") 
  
  Match #1: 
   
  testfile.cpp:1:1: note: "root" binds here 
  int addTwo(int num) 
  ^~~~~~~~~~~~~~~~~~~ 
  testfile.cpp:1:1: note: "x" binds here 
  int addTwo(int num) 
  ^~~~~~~~~~~~~~~~~~~ 
   
  Match #2: 
   
  testfile.cpp:6:1: note: "root" binds here 
  int main(int, char**) 
  ^~~~~~~~~~~~~~~~~~~~~ 
  testfile.cpp:6:1: note: "x" binds here 
  int main(int, char**) 
  ^~~~~~~~~~~~~~~~~~~~~ 
  2 matches. 


clang-query automatically binds ``root`` to the match, but we also bound the name ``x`` to that match. The ``root`` is redundant.  If you ``set bind-root false`` then the output is less noisy:

::

  clang-query> set bind-root false 
  clang-query> m functionDecl().bind("x") 
  
  Match #1: 
  
  testfile.cpp:1:1: note: "x" binds here
  int addtwo(int num) 
  ^~~~~~~~~~~~~~~~~~~ 
  
  Match #2: 
  
  testfile.cpp:6:1: note: "x" binds here 
  int main(int, char**) 
  ^~~~~~~~~~~~~~~~~~~~~ 
  2 matches. 


set print-matcher
~~~~~~~~~~~~~~~~~

``set print-matcher true`` will print a header line of the form 'Matcher: <foo>' where foo is the matcher you have written. It is helpful when debugging multiple matchers at the same time, and no inconvience otherwise.

enable/disable/set output <foo>
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These commands will control the type of output you get from clang-query. The options are:

``print``
  Shows you the C++ form of the node you are matching. This is typically not useful.

``diag``
  Shows you the individual node you are matching. 

``dump`` (alias: ``detailed-ast``)
  Shows you the node you are matching and the entire subtree for the node

By default, you get ``diag`` output. You can change the output by choosing ``set output``. You can *add* output by using ``enable output``. You can *disable* output using ``disable output`` but this is typically not needed.

So if you want to get all three output formats you can do:

::

  # diag output happens automatically because you did not override with 'set'
  enable output print
  enable output dump


Patches
-------

This section tracks some patches; they are currently not used, but we may want them in the future.

- Functionality:

 - `traverse() operator available to clang-query <https://reviews.llvm.org/D80654>`_
 - `srclog output <https://reviews.llvm.org/D93325>`_
 - `allow anyOf() to be empty <https://reviews.llvm.org/D94126>`_
 - breakpoints
 - debug
 - profile

- Matcher Changes:

 - `binaryOperation() matcher <https://reviews.llvm.org/D94129>`_

- Plumbing:

 - `mapAnyOf() <https://reviews.llvm.org/D94127>`_ (`Example of usage <https://reviews.llvm.org/D94131>`_)
 - `Make cxxOperatorCallExpr matchers API-compatible with n-ary operators <https://reviews.llvm.org/D94128>`_
 - `CXXRewrittenBinaryOperator <https://reviews.llvm.org/D94130>`_

