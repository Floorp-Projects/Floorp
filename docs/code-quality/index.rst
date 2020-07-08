Code quality
============

Because Firefox is a complex piece of software, a lot of tools are
executed to identify issues at development phase.
In this document, we try to list these all tools.


.. toctree::
  :maxdepth: 1
  :glob:

  static-analysis.rst
  lint/index.rst
  coding-style/index.rst

.. list-table:: C/C++
   :header-rows: 1
   :widths: 20 20 20 20 20

   * - Tools
     - Has autofixes
     - Meta bug
     - More info
     - Upstream
   * - Custom clang checker
     -
     -
     - `Source <https://searchfox.org/mozilla-central/source/build/clang-plugin>`_
     -
   * - Clang-Tidy
     - Yes
     - `bug 712350 <https://bugzilla.mozilla.org/show_bug.cgi?id=712350>`_
     - :ref:`Static analysis <Mach static analysis>`
     - https://clang.llvm.org/extra/clang-tidy/checks/list.html
   * - Clang analyzer
     -
     - `bug 712350 <https://bugzilla.mozilla.org/show_bug.cgi?id=712350>`_
     -
     - https://clang-analyzer.llvm.org/
   * - Coverity
     -
     - `bug 1230156 <https://bugzilla.mozilla.org/show_bug.cgi?id=1230156>`_
     -
     -
   * - cpp virtual final
     -
     -
     - :ref:`cpp virtual final`
     -
   * - Semmle/LGTM
     -
     - `bug 1458117 <https://bugzilla.mozilla.org/show_bug.cgi?id=1458117>`_
     -
     -
   * - clang-format
     - Yes
     - `bug 1188202 <https://bugzilla.mozilla.org/show_bug.cgi?id=1188202>`_
     - :ref:`Formatting C++ Code With clang-format`
     - https://clang.llvm.org/docs/ClangFormat.html

.. list-table:: JavaScript
   :widths: 20 20 20 20 20
   :header-rows: 1

   * - Tools
     - Has autofixes
     - Meta bug
     - More info
     - Upstream
   * - Eslint
     - Yes
     - `bug 1229856 <https://bugzilla.mozilla.org/show_bug.cgi?id=1229856>`_
     - :ref:`ESLint`
     - https://eslint.org/
   * - Mozilla ESLint
     -
     - `bug 1229856 <https://bugzilla.mozilla.org/show_bug.cgi?id=1229856>`_
     - :ref:`Mozilla ESLint Plugin`
     -
   * - Prettier
     - Yes
     - `bug 1558517 <https://bugzilla.mozilla.org/show_bug.cgi?id=1558517>`_
     - :ref:`JavaScript Coding style`
     - https://prettier.io/



.. list-table:: Python
   :widths: 20 20 20 20 20
   :header-rows: 1

   * - Tools
     - Has autofixes
     - Meta bug
     - More info
     - Upstream
   * - Flake8
     - Yes (with `autopep8 <https://github.com/hhatto/autopep8>`_)
     - `bug 1155970 <https://bugzilla.mozilla.org/show_bug.cgi?id=1155970>`_
     - :ref:`Flake8`
     - http://flake8.pycqa.org/
   * - black
     - Yes
     - `bug 1555560 <https://bugzilla.mozilla.org/show_bug.cgi?id=1555560>`_
     - :ref:`black`
     - https://black.readthedocs.io/en/stable
   * - pylint
     -
     - `bug 1623024 <https://bugzilla.mozilla.org/show_bug.cgi?id=1623024>`_
     - :ref:`pylint`
     - https://www.pylint.org/
   * - Python 2/3 compatibility check
     -
     - `bug 1496527 <https://bugzilla.mozilla.org/show_bug.cgi?id=1496527>`_
     - :ref:`Python 2/3 compatibility check`
     -


.. list-table:: Rust
   :widths: 20 20 20 20 20
   :header-rows: 1

   * - Tools
     - Has autofixes
     - Meta bug
     - More info
     - Upstream
   * - Rustfmt
     - Yes
     - `bug 1454764 <https://bugzilla.mozilla.org/show_bug.cgi?id=1454764>`_
     - :ref:`Rustfmt`
     - https://github.com/rust-lang/rustfmt
   * - Clippy
     -
     - `bug 1361342 <https://bugzilla.mozilla.org/show_bug.cgi?id=1361342>`_
     - :ref:`clippy`
     - https://github.com/rust-lang/rust-clippy

.. list-table:: Java
   :widths: 20 20 20 20 20
   :header-rows: 1

   * - Tools
     - Has autofixes
     - Meta bug
     - More info
     - Upstream
   * - Infer
     -
     - `bug 1175203 <https://bugzilla.mozilla.org/show_bug.cgi?id=1175203>`_
     -
     - https://github.com/facebook/infer

.. list-table:: Others
   :widths: 20 20 20 20 20
   :header-rows: 1

   * - Tools
     - Has autofixes
     - Meta bug
     - More info
     - Upstream
   * - shellcheck
     -
     -
     -
     - https://www.shellcheck.net/
   * - rstchecker
     -
     -
     - :ref:`RST Linter`
     - https://github.com/myint/rstcheck
   * - Typo detection
     - Yes
     -
     - :ref:`Codespell`
     - https://github.com/codespell-project/codespell
   * - YAML linter
     -
     -
     -
     - https://github.com/adrienverge/yamllint

