VIM
===

AutoCompletion
--------------

There's C++ and Rust auto-completion support for VIM via
`YouCompleteMe <https://github.com/ycm-core/YouCompleteMe/>`__. As long as that
is installed and you have run :code:`./mach build` or :code:`./mach configure`,
it should work out of the box. Configuration for this lives in
:code:`.ycm_extra_conf` at the root of the repo.

If you don't like YouCompleteMe, other solutions also work, but they'll require
you to create a :code:`compile_commands.json` file (see below for instructions).

Rust auto-completion should work both with the default completer (RLS, as of
this writing), or with `rust-analyzer <https://rust-analyzer.github.io/manual.html#youcompleteme>`__.

ESLint
------

The easiest way to integrate ESLint with VIM is using the `Syntastic plugin
<https://github.com/vim-syntastic/syntastic>`__.

In order for VIM to detect jsm files as JS you might want something like this
in your :code:`.vimrc`:

.. code::

    autocmd BufRead,BufNewFile *.jsm set filetype=javascript

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
