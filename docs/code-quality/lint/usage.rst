Running Linters Locally
=======================

Using the Command Line
----------------------

You can run all the various linters in the tree using the ``mach lint`` command. Simply pass in the
directory or file you wish to lint (defaults to current working directory):

.. parsed-literal::

    ./mach lint path/to/files

Multiple paths are allowed:

.. parsed-literal::

    ./mach lint path/to/foo.js path/to/bar.py path/to/dir

To force execution on a directory that would otherwise be excluded:

.. parsed-literal::

    ./mach lint -n path/in/the/exclude/list

``Mozlint`` will automatically determine which types of files exist, and which linters need to be run
against them. For example, if the directory contains both JavaScript and Python files then mozlint
will automatically run both ESLint and Flake8 against those files respectively.

To restrict which linters are invoked manually, pass in ``-l/--linter``:

.. parsed-literal::

    ./mach lint -l eslint path/to/files

You can see a list of the available linters by running:

.. parsed-literal::

    ./mach lint --list

Finally, ``mozlint`` can lint the files touched by outgoing revisions or the working directory using
the ``-o/--outgoing`` and ``-w/--workdir`` arguments respectively. These work both with mercurial and
git. In the case of ``--outgoing``, the default remote repository the changes would be pushed to is
used as the comparison. If desired, a remote can be specified manually. In git, you may only want to
lint staged commits from the working directory, this can be accomplished with ``--workdir=staged``.
Examples:

.. parsed-literal::

    ./mach lint --workdir
    ./mach lint --workdir=staged
    ./mach lint --outgoing
    ./mach lint --outgoing origin/master
    ./mach lint -wo

.. _lint-vcs-hook:

Using a VCS Hook
----------------

There are also both pre-commit and pre-push version control hooks that work in
either hg or git. To enable a pre-push hg hook, add the following to hgrc:

.. parsed-literal::

    [hooks]
    pre-push.lint = python:./tools/lint/hooks.py:hg


To enable a pre-commit hg hook, add the following to hgrc:

.. parsed-literal::

    [hooks]
    pretxncommit.lint = python:./tools/lint/hooks.py:hg


To enable a pre-push git hook, run the following command:

.. parsed-literal::

    $ ln -s /path/to/gecko/tools/lint/hooks.py .git/hooks/pre-push


To enable a pre-commit git hook, run the following command:

.. parsed-literal::

    $ ln -s /path/to/gecko/tools/lint/hooks.py .git/hooks/pre-commit


Fixing Lint Errors
==================

``Mozlint`` has a best-effort ability to fix lint errors:

.. parsed-literal::

    $ ./mach lint --fix

Not all linters support fixing, and even the ones that do can not usually fix
all types of errors. Any errors that cannot be automatically fixed, will be
printed to stdout like normal. In that case, you can also fix errors manually:

.. parsed-literal::

    $ ./mach lint --edit

This requires the $EDITOR environment variable be defined. For most editors,
this will simply open each file containing errors one at a time. For vim (or
neovim), this will populate the `quickfix list`_ with the errors.

The ``--fix`` and ``--edit`` arguments can be combined, in which case any
errors that can be fixed automatically will be, and the rest will be opened
with your $EDITOR.

Editor Integration
==================

Editor integrations are highly recommended for linters, as they let you see
errors in real time, and can help you fix issues before you compile or run tests.

Although mozilla-central does not currently have an integration available for
`./mach lint`, there are various integrations available for some of the major
linting tools that we use:

* `ESLint`_
* `Black (Python)`_

.. _quickfix list: http://vimdoc.sourceforge.net/htmldoc/quickfix.html
.. _ESLint: https://eslint.org/docs/user-guide/integrations#editors
.. _Black (Python): https://black.readthedocs.io/en/stable/editor_integration.html
