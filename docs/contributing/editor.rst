Editor / IDE integration
========================

You can use any editor or IDE to contribute to Firefox, as long as it can edit
text files. However, there are some steps specific to mozilla-central that may
be useful for a better development experience. This page attempts to document
them.

.. note::

    This page is a work in progress. Please enhance this page with instructions
    for your favourite editor.

Visual Studio Code
------------------

For general information on using VS Code, see their `home page <https://code.visualstudio.com/>`__, `repo <https://github.com/Microsoft/vscode/>`__ and `guide to working with C++ <https://code.visualstudio.com/docs/languages/cpp>`__.

For IntelliSense to work properly, a :ref:`compilation database <CompileDB back-end / compileflags>` as described below is required. When it is present when you open the mozilla source code folder, it will be automatically detected and Visual Studio Code will ask you if it should use it, which you should confirm.

VS Code provides number of extensions for JavaScript, Rust, etc.

Useful preferences
^^^^^^^^^^^^^^^^^^

When setting the preference

.. code::

  "editor.formatOnSave": true

you might find that this isn't working on large source code files, but triggering formatting manually works. This is due to the default timeout for formatOnSave, which is quite short (750ms). You might want to increase this timeout, e.g.

.. code::

   "editor.formatOnSaveTimeout": 5000

VIM
---

AutoCompletion
~~~~~~~~~~~~~~

There's C++ and Rust auto-completion support for VIM via
`YouCompleteMe <https://github.com/ycm-core/YouCompleteMe/>`__.

As long as that is installed and you have run :code:`./mach build` or
:code:`./mach configure`, it should work out of the box. Configuration for this lives
in :code:`.ycm_extra_conf` at the root of the repo.

ESLint
~~~~~~

The easiest way to integrate ESLint with VIM is using the `Syntastic plugin
<https://github.com/vim-syntastic/syntastic>`__.

:code:`mach eslint --setup` installs a specific ESLint version and some ESLint
plugins into the repositories' :code:`node_modules`.

You need something like this in your :code:`.vimrc` to run the checker
automatically on save:

.. code::

    autocmd FileType javascript,html,xhtml let b:syntastic_checkers = ['javascript/eslint']

You need to have :code:`eslint` in your :code:`PATH`, which you can get with
:code:`npm install -g eslint`. You need at least version 6.0.0.

You can also use something like `eslint_d
<https://github.com/mantoni/eslint_d.js#editor-integration>`__ which should
also do that automatically:

.. code::

    let g:syntastic_javascript_eslint_exec = 'eslint_d'


Eclipse
-------

You can generate an Eclipse project by running:

.. code::

    ./mach ide eclipse

Visual Studio
-------------

You can run a Visual Studio project by running:

.. code::

    ./mach ide visualstudio

CompileDB back-end / compileflags
---------------------------------

You can generate a :code:`compile_commands.json` in your object directory by
running:

.. code::

    ./mach build-backend --backend=CompileDB

This file, the compilation database, is understood by a variety of C++ editors / IDEs
to provide auto-completion capabilities. You can also get an individual compile command by
running:

.. code::

    ./mach compileflags path/to/file

This is how the :ref:`VIM <VIM>` integration works, for example.
