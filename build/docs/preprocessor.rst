.. _preprocessor:

=================
Text Preprocessor
=================

The build system contains a text preprocessor similar to the C preprocessor,
meant for processing files which have no built-in preprocessor such as XUL
and JavaScript documents. It is implemented at ``python/mozbuild/mozbuild/preprocessor.py`` and
is typically invoked via :ref:`jar_manifests`.

While used to preprocess CSS files, the directives are changed to begin with
``%`` instead of ``#`` to avoid conflict of the id selectors.

Directives
==========

Variable Definition
-------------------

define
^^^^^^

::

   #define variable
   #define variable value

Defines a preprocessor variable.

Note that, unlike the C preprocessor, instances of this variable later in the
source are not automatically replaced (see #filter). If value is not supplied,
it defaults to ``1``.

Note that whitespace is significant, so ``"#define foo one"`` and
``"#define foo one "`` is different (in the second case, ``foo`` is defined to
be a four-character string).

undef
^^^^^

::

   #undef variable

Undefines a preprocessor variable.

Conditionals
------------

if
^^

::

   #if variable
   #if !variable
   #if variable == string
   #if variable != string

Disables output if the conditional is false. This can be nested to arbitrary
depths. Note that in the equality checks, the variable must come first.

else
^^^^

::

   #else

Reverses the state of the previous conditional block; for example, if the
last ``#if`` was true (output was enabled), an ``#else`` makes it off
(output gets disabled).

endif
^^^^^

::

   #endif

Ends the conditional block.

ifdef / ifndef
^^^^^^^^^^^^^^

::

   #ifdef variable
   #ifndef variable

An ``#if`` conditional that is true only if the preprocessor variable
variable is defined (in the case of ``ifdef``) or not defined (``ifndef``).

elif / elifdef / elifndef
^^^^^^^^^^^^^^^^^^^^^^^^^

::

   #elif variable
   #elif !variable
   #elif variable == string
   #elif variable != string
   #elifdef variable
   #elifndef variable

A shorthand to mean an ``#else`` combined with the relevant conditional.
The following two blocks are equivalent::

   #ifdef foo
     block 1
   #elifdef bar
     block 2
   #endif

::

   #ifdef foo
     block 1
   #else
   #ifdef bar
     block 2
   #endif
   #endif

File Inclusion
--------------

include
^^^^^^^

::

   #include filename

The file specified by filename is processed as if the contents was placed
at this position. This also means that preprocessor conditionals can even
be started in one file and ended in another (but is highly discouraged).
There is no limit on depth of inclusion, or repeated inclusion of the same
file, or self inclusion; thus, care should be taken to avoid infinite loops.

includesubst
^^^^^^^^^^^^

::

   #includesubst @variable@filename

Same as a ``#include`` except that all instances of variable in the included
file is also expanded as in ``#filter`` substitution

expand
^^^^^^

::

   #expand string

All variables wrapped in ``__`` are replaced with their value, for this line
only. If the variable is not defined, it expands to an empty string. For
example, if ``foo`` has the value ``bar``, and ``baz`` is not defined, then::

   #expand This <__foo__> <__baz__> gets expanded

Is expanded to::

   This <bar> <> gets expanded

filter / unfilter
^^^^^^^^^^^^^^^^^

::

   #filter filter1 filter2 ... filterN
   #unfilter filter1 filter2 ... filterN

``#filter`` turns on the given filter.

Filters are run in alphabetical order on a per-line basis.

``#unfilter`` turns off the given filter. Available filters are:

emptyLines
   strips blank lines from the output
slashslash
   strips everything from the first two consecutive slash (``/``)
   characters until the end of the line
spaces
   collapses consecutive sequences of spaces into a single space,
   and strips leading and trailing spaces
substitution
   all variables wrapped in @ are replaced with their value. If the
   variable is not defined, it is a fatal error. Similar to ``#expand``
   and ``#filter``
attemptSubstitution
   all variables wrapped in ``@`` are replaced with their value, or an
   empty string if the variable is not defined. Similar to ``#expand``.

literal
^^^^^^^

::

   #literal string

Output the string (i.e. the rest of the line) literally, with no other fixups.
This is useful to output lines starting with ``#``, or to temporarily
disable filters.

Other
-----

#error
^^^^^^

::

   #error string

Cause a fatal error at this point, with the error message being the
given string.
