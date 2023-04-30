Vim / Neovim
============

AutoCompletion
--------------

For C++, anything that can use an LSP like :code:`coc.nvim`,
:code:`nvim-lspconfig`, or what not, should work as long as you generate a
:ref:`compilation database <CompileDB back-end-compileflags>` and point to it.

Additionally, `YouCompleteMe <https://github.com/ycm-core/YouCompleteMe/>`__
works without the need of a C++ compilation database as long as you have run
:code:`./mach build` or :code:`./mach configure`. Configuration for this lives
in :searchfox:`.ycm_extra_conf <.ycm_extra_conf>` at the root of the repo.

Rust auto-completion should work both with Rust's LSP :code:`rust-analyzer`.

Make sure that the LSP is configured in a way that it detects the root of the
tree as a workspace, not the crate you happen to be editing. For example, the
default of :code:`nvim-lspconfig` is to search for the closest
:code:`Cargo.toml` file, which is not what you want. You'd want something like:

.. code ::

    root_dir = lspconfig.util.root_pattern(".git", ".hg")

You also need to set some options to get full diagnostics:

.. code ::

   "rust-analyzer.server.extraEnv": {
     "CARGO_TARGET_DIR": "/path/to/objdir"
   },
   "rust-analyzer.check.overrideCommand": [ "/path/to/mach", "--log-no-times", "cargo", "check", "--all-crates", "--message-format-json" ],
   "rust-analyzer.cargo.buildScripts.overrideCommand": [ "/path/to/mach", "--log-no-times", "cargo", "check", "--all-crates", "--message-format-json" ],

The easiest way to make these work out of the box is using
`neoconf <https://github.com/folke/neoconf.nvim/>`__, which
automatically supports importing VSCode configuration files.
:code:`./mach ide vscode --no-interactive` will then generate the right
configuration for you.

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
