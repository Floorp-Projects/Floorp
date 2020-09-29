mach
====

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

`Mach <https://hg.mozilla.org/mozilla-central/file/tip/mach>`__ (German
for *to make*) is a program via the "command-line interface" to help
developers perform installation tasks such as installing Firefox from
its C++ source code.

Requirements
------------

Mach requires a current version of /mozilla-central/ (or a tree derived
from there). Mach also requires Python 2.7. Mach itself is Python 3 compliant,
but modules used by mach are not Python 3 compliant - so stick to Python 2.7.
See the meta `bug 1577599 <https://bugzilla.mozilla.org/show_bug.cgi?id=1577599>`__
for more information.

Running
-------

From the root of the source tree checkout, you should just be able to
type:

.. code-block:: shell

   $ ./mach

If all is well, you should see a help message. Of course, in a random
dir you just get a cryptic error message to improve the experience.

For full help:

.. code-block:: shell

   $ ./mach help

Try building the tree:

.. code-block:: shell

   $ ./mach build

If you get error messages, make sure that you have all of the :ref:`build
requisites <Getting Set Up To Work On The Firefox Codebase>` for your system.

If it works, you can look at compiler warnings:

.. code-block:: shell

   $ ./mach warnings-list

Try running the program:

.. code-block:: shell

   $ ./mach run

Try running your program in a debugger:

.. code-block:: shell

   $ ./mach run --debug

Try running some tests:

.. code-block:: shell

   $ ./mach xpcshell-test services/common/tests/unit/

Or run an individual test:

.. code-block:: shell

   $ ./mach mochitest browser/base/content/test/general/browser_pinnedTabs.js

You run mach from the source directory, so you should be able to use
your shell's tab completion to tab-complete paths to tests. Mach figures
out how to execute the tests for you!

Check out the :ref:`linting` and :ref:`Static analysis` tools:

.. code-block:: shell

   $ ./mach lint
   $ ./mach static-analysis

.. _mach_and_mozconfigs:

mach and mozconfigs
~~~~~~~~~~~~~~~~~~~

It's possible to use mach with multiple mozconfig files. mach's logic
for determining which mozconfig to use is effectively the following:

#. If a .mozconfig file (some say it is the file **mozconfig** without
   the dot) exists in the current directory, use that.
#. If the ``MOZCONFIG``\ environment variable is set, use the file
   pointed to in that variable.
#. If the current working directory mach is invoked with is inside an
   object directory, the mozconfig used when creating that object
   directory is used.
#. The default mozconfig search logic is applied.

Here are some examples:

.. code-block:: shell

   # Use an explicit mozconfig file.
   $ MOZCONFIG=/path/to/mozconfig ./mach build

   # Alternatively (for persistent mozconfig usage):
   $ export MOZCONFIG=/path/to/mozconfig
   $ ./mach build

   # Let's pretend the MOZCONFIG environment variable isn't set. This will use
   # the mozconfig from the object directory.
   $ cd objdir-firefox
   $ mach build

.. _Adding_mach_to_your_shell:

Adding mach to your shell's search path
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you add mach to your path (by modifying the ``PATH`` environment
variable to include your source directory, or by copying ``mach``\ to a
directory in the default path like ``/usr/local/bin``) then you can type
``mach``\ anywhere in your source directory or your
:ref:`objdir <Configuring Build Options>`.  Mach expands
relative paths starting from the current working directory, so you can
run commands like ``mach build .`` to rebuild just the files in the
current directory.  For example:

.. code-block:: shell

   $ cd devtools/client
   $ mach build webconsole # Rebuild only the files in the devtools/client/webconsole directory
   $ mach mochitest webconsole/test # Run mochitests in devtools/client/webconsole/test


Enable tab completion
~~~~~~~~~~~~~~~~~~~~~

To enable tab completion in ``bash``, run the following command.  You
can add the command to your ``.profile`` so it will run automatically
when you start the shell:

.. code-block:: shell

   source /path/to/mozilla-central/python/mach/bash-completion.sh

This will enable tab completion of mach command names, and in the future
it may complete flags and other arguments too.  Note: Mach tab
completion will not work when running mach in a source directory older
than Firefox 24.

For zsh, you can call the built-in bashcompinit function before
sourcing:

.. code-block:: shell

   autoload bashcompinit
   bashcompinit
   source /path/to/mozilla-central/python/mach/bash-completion.sh


Frequently Asked Questions
--------------------------

How do I report bugs?
~~~~~~~~~~~~~~~~~~~~~

Bugs against the mach core can be filed in Bugzilla in the `Firefox
Build System::Mach
Core <https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox%20Build%20System&component=Mach%20Core>`__.

.. note::

   Most mach bugs are bugs in individual commands, not bugs in the core
   mach code. Bugs for individual commands should be filed against the
   component that command is related to. For example, bugs in the
   *build* command should be filed against *Firefox Build System ::
   General*. Bugs against testing commands should be filed somewhere in
   the *Testing* product.


How is building with mach different from building with client.mk, from using make directly?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Currently, ``mach build`` simply invokes client.mk as an ersatz
Makefile. **There are no differences in terms of how the build is
performed** (well, at least there should not be any ideally). However -
mach does offer some additional features over manual invocation of
client.mk:

-  If on Windows, mach will automatically use pymake instead of GNU
   make, as that is preferred on Windows.
-  mach will print timings with each line of output from the build. This
   gives you an idea of how long things take.
-  mach will colorize terminal output (on terminals that support it -
   typically most terminals except on Windows)
-  mach will scan build output for compiler warnings and will
   automatically record them to a database which can be queried with
   ``mach warnings-list`` and ``mach warnings-summary``. Not all
   warnings are currently detected. Do not rely on mach as a substitute
   for raw build output.
-  mach will invoke make in silent mode. This suppresses excessive
   (often unnecessary) output.


Is mach a build system?
~~~~~~~~~~~~~~~~~~~~~~~

No. Mach is just a generic command dispatching tool that happens to have
a few commands that interact with the real build system. Historically,
mach *was* born to become a better interface to the build system.
However, its potential beyond just build system interaction was quickly
realized and mach grew to fit those needs. Generally, Mozilla wants to
move to a python-based build system but the transition period will be
rather long.


Does mach work with mozconfigs?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes! You use the control file **mozconfig** like you have always used
them.


Does mach have its own configuration file?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes. You can specify configuration settings in a ``~/.mozbuild/machrc``
file. To see the list of the 4 available settings ( which are:  alias ,
test , try , runprefs )  type :

.. code-block:: shell

   $ ./mach settings


Should I implement X as a mach command?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are no hard or fast rules. Generally speaking, if you have some
piece of functionality or action that is useful to multiple people
(especially if it results in productivity wins), then you should
consider implementing a mach command for it.

Some other cases where you should consider implementing something as a
mach command:

-  When your tool is a random script in the tree. Random scripts are
   hard to find and may not conform to coding conventions or best
   practices. Mach provides a framework in which your tool can live that
   will put it in a better position to succeed than if it were on its
   own.
-  When the alternative is a make target. The build team generally does
   not like one-off make targets that aren't part of building (read:
   compiling) the tree. This includes things related to testing and
   packaging. These weigh down make files and add to the burden of
   maintaining the build system. Instead, you are encouraged to
   implement ancillary functionality in *not make* (preferably Python).
   If you do implement something in Python, hooking it up to mach is
   often trivial (just a few lines of proxy code).


How does mach fit into the modules system?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mozilla operates with a `modules governance
system <https://www.mozilla.org/about/governance/policies/module-ownership/>`__ where
there are different components with different owners. There is not
currently a mach module. There may or may never be one. Mach is just a
generic tool. The mach core is the only thing that could fall under
perview of a module and an owner.

Even if a mach module were established, mach command modules (see below)
would likely never belong to it. Instead, mach command modules are owned
by the team/module that owns the system they interact with. In other
words, mach is not a power play to consolidate authority for tooling.
Instead, it aims to expose that tooling through a common, shared
interface.


Who do I contact for help or to report issues?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can ask questions in `#build <https://chat.mozilla.org/#/room/#build:mozilla.org>`__.


Can I use mach outside of mozilla-central?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Yes, the mach core is in `mozilla-central inside the python/mach
directory <https://hg.mozilla.org/mozilla-central/file/default/python/mach/>`__
and available on PyPI at https://pypi.python.org/pypi/mach/. The actual
file "mach" (a py script) , which you need, is not there though - look
for that driver `at
moz-central <https://hg.mozilla.org/mozilla-central/file/default/mach>`__
or `here
directly <https://hg.mozilla.org/mozilla-central/raw-file/tip/mach>`__.


mach Architecture
-----------------

Under the hood mach is a generic command dispatching framework which
currently targets command line interfaces (CLIs). You essentially have a
bunch of Python functions saying "I provide command X" and mach hooks up
command line argument parsing, terminal interaction, and dispatching.

There are 3 main components to mach:

#. The mach core.
#. Mach commands
#. The mach driver

The mach core is the main Python modules that implement the basic
functionality of mach. These include command line parsing, a structured
logger, dispatching, and utility functions to aid in the implementation
of mach commands.

Mach commands are what actually perform work when you run mach. Mach has
a few built-in commands. However, most commands aren't part of mach
itself. Instead, they are registered with mach.

The mach driver is the mach command line interface. It's a Python script
that creates an instance of the mach core, registers commands with it,
then tells the mach core to execute.

The canonical source repository for the mach core is the
`python/mach <https://hg.mozilla.org/mozilla-central/file/default/python/mach/>`__
directory in mozilla-central. The main mach routine lives in
`main.py <https://hg.mozilla.org/mozilla-central/file/default/python/mach/mach/main.py>`__.
The mach driver is the
`mach <https://hg.mozilla.org/mozilla-central/file/default/mach>`__ file
in the root directory of mozilla-central. As you can see, the mach
driver is a shim that calls into the mach core.

As you may have inferred, mach is implemented in Python. Python is our
tooling programming language of choice at Mozilla. Mach is also Python 3
compliant (at least it should be).

.. _Adding_Features_to_mach:

Adding Features to mach
-----------------------

Most mach features come in the form of new commands. Implementing new
commands is as simple as writing a few lines of Python and registering
the created file with mach.

The first step to adding a new feature to mach is to file a bug. You
have the choice of filing a bug in the ``Core :: mach`` component or in
any other component. If you file outside of ``Core :: mach``, please add
``[mach]`` to the whiteboard.

Mach is relatively new and the API is changing. So, the best way to
figure out how to implement a new mach command is probably to look at an
existing one.

Start by looking at the source for the `mach
driver <https://hg.mozilla.org/mozilla-central/file/default/mach>`__.
You will see a list defining paths to Python files (likely named
``mach_commands.py``). These are the Python files that implement mach
commands and are loaded by the mach driver. These are relative paths in
the source repository. Simply find one you are interested in and dig in!

.. _mach_Command_Providers:

mach Command Providers
~~~~~~~~~~~~~~~~~~~~~~

A mach command provider is simply a Python module. When these modules
are loaded, mach looks for specific signatures to detect mach commands.
Currently, this is implemented through Python decorators. Here is a
minimal mach command module:

.. code:: brush:

   from __future__ import print_function, unicode_literals

   from mach.decorators import (
       CommandArgument,
       CommandProvider,
       Command,
   )

   @CommandProvider
   class MachCommands(object):
       @Command('doit', description='Run it!')
       @CommandArgument('--debug', '-d', action='store_true',
           help='Do it in debug mode.')
       def doit(self, debug=False):
           print('I did it!')

From ``mach.decorators`` we import some Python decorators which are used
to define what Python code corresponds to mach commands.

The decorators are:

@CommandProvider
   This is a class decorator that tells mach that this class contains
   methods that implement mach commands. Without this decorator, mach
   will not know about any commands defined within, even if they have
   decorators.
@Command
   This is a method decorator that tells mach that this method
   implements a mach command. The arguments to the decorator are those
   that can be passed to the
   ```argparse.ArgumentParser`` <http://docs.python.org/library/argparse.html#sub-commands>`__
   constructor by way of sub-commands.
@CommandArgument
   This is a method decorator that tells mach about an argument to a
   mach command. The arguments to the decorator are passed to
   ```argparse.ArgumentParser.add_argument()`` <http://docs.python.org/library/argparse.html#argparse.ArgumentParser.add_argument>`__.

The class and method names can be whatever you want. They are irrelevant
to mach.

An instance of the ``@CommandProvider`` class is instantiated by the
mach driver if a command in it is called for execution. The ``__init__``
method of the class must take either 1 or 2 arguments (including
``self``). If your class inherits from ``object``, no explicit
``__init__`` implementation is required (the default takes 1 argument).
If your class's ``__init__`` takes 2 arguments, the second argument will
be an instance of ``mach.base.CommandContext``. This object holds state
from the mach driver, including the current directory, a handle on the
logging manager, the settings object, and information about available
mach commands.

The arguments registered with @CommandArgument are passed to your method
as keyword arguments using the ``**kwargs`` calling convention. So, you
should define default values for all of your method's arguments.

The return value from the @Command method should be the integer exit
code from the process. If not defined or None, 0 will be used.


Registering mach Command Providers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once you've written a Python module providing a mach command, you'll
need to register it with mach. There are two ways to do this.

If you have a single file, the easiest solution is probably to register
it as a one-off inside ``build/mach_bootstrap.py``. There should be a
Python list of paths named ``MACH_MODULES`` or similar. Just add your
file to that list, run ``mach help`` and your new command should appear!


Submitting a mach Command for Approval
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once you've authored a mach command, submit the patch for approval.
Please flag firefox-build-system-reviewers for review.


Mach Command Modules Useful Information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Command modules are not imported into a reliable Python package/module
"namespace." Therefore, you can't rely on the module name. All imports
must be absolute, not relative.

Because mach command modules are loaded at mach start-up, it is
important that they be lean and not have a high import cost. This means
that you should avoid global ``import`` statements as much as possible.
Instead, defer your import until inside the ``@Command`` decorated
method.

Mach ships with a toolbox of mix-in classes to facilitate common
actions. See
```python/mach/mach/mixin`` <https://hg.mozilla.org/mozilla-central/file/default/python/mach/mach/mixin>`__.
If you find yourself reinventing the wheel or doing something you feel
that many mach commands will want to do, please consider authoring a new
mix-in class so your effort can be shared!

.. _See_also:

See also
--------

-  `Mach in the Mozilla source tree
   docs <https://gfritzsche-demo.readthedocs.io/en/latest/python/mach/index.html>`__
-  `Mach PyPi page <https://pypi.org/project/mach/>`__
