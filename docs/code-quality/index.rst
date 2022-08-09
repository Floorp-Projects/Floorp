Code quality
============

Because Firefox is a complex piece of software, a lot of tools are
executed to identify issues at development phase.
In this document, we try to list these all tools.


.. toctree::
  :maxdepth: 1
  :glob:

  static-analysis/index.rst
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
     - `bug 712350 <https://bugzilla.mozilla.org/show_bug.cgi?id=712350>`__
     - :ref:`Static analysis <Static Analysis>`
     - https://clang.llvm.org/extra/clang-tidy/checks/list.html
   * - Clang analyzer
     -
     - `bug 712350 <https://bugzilla.mozilla.org/show_bug.cgi?id=712350>`__
     -
     - https://clang-analyzer.llvm.org/
   * - cpp virtual final
     -
     -
     - :ref:`cpp virtual final`
     -
   * - Semmle/LGTM
     -
     - `bug 1458117 <https://bugzilla.mozilla.org/show_bug.cgi?id=1458117>`__
     -
     -
   * - clang-format
     - Yes
     - `bug 1188202 <https://bugzilla.mozilla.org/show_bug.cgi?id=1188202>`__
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
     - `bug 1229856 <https://bugzilla.mozilla.org/show_bug.cgi?id=1229856>`__
     - :ref:`ESLint`
     - https://eslint.org/
   * - Mozilla ESLint
     -
     - `bug 1229856 <https://bugzilla.mozilla.org/show_bug.cgi?id=1229856>`__
     - :ref:`Mozilla ESLint Plugin`
     -
   * - Prettier
     - Yes
     - `bug 1558517 <https://bugzilla.mozilla.org/show_bug.cgi?id=1558517>`__
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
     - `bug 1155970 <https://bugzilla.mozilla.org/show_bug.cgi?id=1155970>`__
     - :ref:`Flake8`
     - http://flake8.pycqa.org/
   * - black
     - Yes
     - `bug 1555560 <https://bugzilla.mozilla.org/show_bug.cgi?id=1555560>`__
     - :ref:`black`
     - https://black.readthedocs.io/en/stable
   * - pylint
     -
     - `bug 1623024 <https://bugzilla.mozilla.org/show_bug.cgi?id=1623024>`__
     - :ref:`pylint`
     - https://www.pylint.org/
   * - Python 2/3 compatibility check
     -
     - `bug 1496527 <https://bugzilla.mozilla.org/show_bug.cgi?id=1496527>`__
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
     - `bug 1454764 <https://bugzilla.mozilla.org/show_bug.cgi?id=1454764>`__
     - :ref:`Rustfmt`
     - https://github.com/rust-lang/rustfmt
   * - Clippy
     -
     - `bug 1361342 <https://bugzilla.mozilla.org/show_bug.cgi?id=1361342>`__
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
   * - Spotless
     - Yes
     - `bug 1571899 <https://bugzilla.mozilla.org/show_bug.cgi?id=1571899>`__
     - :ref:`Spotless`
     - https://github.com/diffplug/spotless

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
   * - Fluent Lint
     - No
     -
     - :ref:`Fluent Lint`
     -
   * - YAML linter
     - No
     -
     - :ref:`yamllint`
     - https://github.com/adrienverge/yamllint
