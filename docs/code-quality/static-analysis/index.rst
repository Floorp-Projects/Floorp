Static Analysis
===============

Static Analysis is running an analysis of the source code without actually executing the code. For the most part, at Mozilla static analysis refers to the stuff we do with `clang-tidy <http://clang.llvm.org/extra/clang-tidy/>`__. It uses
checkers in order to prevent different programming errors present in the
code. The checkers that we use are split into 3 categories:

#. :searchfox:`Firefox specific checkers <build/clang-plugin>`. They detect incorrect Gecko programming
   patterns which could lead to bugs or security issues.
#. `Clang-tidy checkers <https://clang.llvm.org/extra/clang-tidy/checks/list.html>`_. They aim to suggest better programming practices
   and to improve memory efficiency and performance.
#. `Clang-analyzer checkers <https://clang-analyzer.llvm.org/>`_. These checks are more advanced, for example
   some of them can detect dead code or memory leaks, but as a typical
   side effect they have false positives. Because of that, we have
   disabled them for now, but will enable some of them in the near
   future.

In order to simplify the process of static-analysis we have focused on
integrating this process with Phabricator and mach. A list of some
checkers that are used during automated scan can be found
:searchfox:`here <tools/clang-tidy/config.yaml>`.

This documentation is split into two parts:

.. toctree::
  :maxdepth: 1
  :glob:

  existing.rst
  writing-new/index.rst
