
# Writing Commands

## Basics

GCLI has opinions about how commands should be written, and it encourages you
to do The Right Thing. The opinions are based on helping users convert their
intentions to commands and commands to what's actually going to happen.

- Related commands should be sub-commands of a parent command. One of the goals
  of GCLI is to support a large number of commands without things becoming
  confusing, this will require some sort of namespacing or there will be
  many people wanting to implement the ``add`` command. This style of
  writing commands has become common place in Unix as the number of commands
  has gone up.
  The ```context``` command allows users to focus on a parent command, promoting
  its sub-commands above others.

- Each command should do exactly and only one thing. An example of a Unix
  command that breaks this principle is the ``tar`` command

        $ tar -zcf foo.tar.gz .
        $ tar -zxf foo.tar.gz .

  These 2 commands do exactly opposite things. Many a file has died as a result
  of a x/c typo. In GCLI this would be better expressed:

        $ tar create foo.tar.gz -z .
        $ tar extract foo.tar.gz -z .

  There may be commands (like tar) which have enough history behind them
  that we shouldn't force everyone to re-learn a new syntax. The can be achieved
  by having a single string parameter and parsing the input in the command)

- Avoid errors. We try to avoid the user having to start again with a command
  due to some problem. The majority of problems are simple typos which we can
  catch using command metadata, but there are 2 things command authors can do
  to prevent breakage.

  - Where possible avoid the need to validate command line parameters in the
    exec function. This can be done by good parameter design (see 'do exactly
    and only one thing' above)

  - If there is an obvious fix for an unpredictable problem, offer the
    solution in the command output. So rather than use request.error (see
    Request Object below) output some HTML which contains a link to a fixed
    command line.

Currently these concepts are not enforced at a code level, but they could be in
the future.


## How commands work

This is how to create a basic ``greet`` command:

    gcli.addItems([{
      name: 'greet',
      description: 'Show a greeting',
      params: [
        {
          name: 'name',
          type: 'string',
          description: 'The name to greet'
        }
      ],
      returnType: 'string',
      exec: function(args, context) {
        return 'Hello, ' + args.name;
      }
    }]);

This command is used as follows:

    : greet Joe
    Hello, Joe

Some terminology that isn't always obvious: a function has 'parameters', and
when you call a function, you pass 'arguments' to it.


## Internationalization (i18n)

There are several ways that GCLI commands can be localized. The best method
depends on what context you are writing your command for.

### Firefox Embedding

GCLI supports Mozilla style localization. To add a command that will only ever
be used embedded in Firefox, this is the way to go. Your strings should be
stored in ``toolkit/locales/en-US/chrome/global/devtools/gclicommands.properties``,
And you should access them using ``let l10n = require("gcli/l10n")`` and then
``l10n.lookup(...)`` or ``l10n.lookupFormat()``

For examples of existing commands, take a look in
``browser/devtools/webconsole/GcliCommands.jsm``, which contains most of the
current GCLI commands. If you will be adding a number of new commands, then
consider starting a new JSM.

Your command will then look something like this:

    gcli.addItems([{
      name: 'greet',
      description: gcli.lookup("greetDesc")
      ...
    }]);

### Web Commands

There are 2 ways to provide translated strings for web use. The first is to
supply the translated strings in the description:

    gcli.addItems([{
      name: 'greet',
      description: {
        'root': 'Show a greeting',
        'fr-fr': 'Afficher un message d'accueil',
        'de-de': 'Zeige einen Gruß',
        'gk-gk': 'Εμφάνιση ένα χαιρετισμό',
        ...
      }
      ...
    }]);

Each description should contain at least a 'root' entry which is the
default if no better match is found. This method has the benefit of being
compact and simple, however it has the significant drawback of being wasteful
of memory and bandwidth to transmit and store a significant number of strings,
the majority of which will never be used.

More efficient is to supply a lookup key and ask GCLI to lookup the key from an
appropriate localized strings file:

    gcli.addItems([{
      name: 'greet',
      description: { 'key': 'demoGreetingDesc' }
      ...
    }]);

For web usage, the central store of localized strings is
``lib/gcli/nls/strings.js``. Other string files can be added using the
``l10n.registerStringsSource(...)`` function.

This method can be used both in Firefox and on the Web (see the help command
for an example). However this method has the drawback that it will not work
with DryIce built files until we fix bug 683844.


## Default argument values

The ``greet`` command requires the entry of the ``name`` parameter. This
parameter can be made optional with the addition of a ``defaultValue`` to the
parameter:

    gcli.addItems([{
      name: 'greet',
      description: 'Show a message to someone',
      params: [
        {
          name: 'name',
          type: 'string',
          description: 'The name to greet',
          defaultValue: 'World!'
        }
      ],
      returnType: 'string',
      exec: function(args, context) {
        return "Hello, " + args.name;
      }
    }]);

Now we can also use the ``greet`` command as follows:

    : greet
    Hello, World!


## Positional vs. named arguments

Arguments can be entered either positionally or as named arguments. Generally
users will prefer to type the positional version, however the named alternative
can be more self documenting.

For example, we can also invoke the greet command as follows:

    : greet --name Joe
    Hello, Joe


## Short argument names

GCLI allows you to specify a 'short' character for any parameter:

    gcli.addItems([{
      name: 'greet',
      params: [
        {
          name: 'name',
          short: 'n',
          type: 'string',
          ...
        }
      ],
      ...
    }]);

This is used as follows:

    : greet -n Fred
    Hello, Fred

Currently GCLI does not allow short parameter merging (i.e. ```ls -la```)
however this is planned.


## Parameter types

Initially the available types are:

- string
- boolean
- number
- selection
- delegate
- date
- array
- file
- node
- nodelist
- resource
- command
- setting

This list can be extended. See [Writing Types](writing-types.md) on types for
more information.

The following examples assume the following definition of the ```greet```
command:

    gcli.addItems([{
      name: 'greet',
      params: [
        { name: 'name', type: 'string' },
        { name: 'repeat', type: 'number' }
      ],
      ...
    }]);

Parameters can be specified either with named arguments:

    : greet --name Joe --repeat 2

And sometimes positionally:

    : greet Joe 2

Parameters can be specified positionally if they are considered 'important'.
Unimportant parameters must be specified with a named argument.

Named arguments can be specified anywhere on the command line (after the
command itself) however positional arguments must be in order. So
these examples are the same:

    : greet --name Joe --repeat 2
    : greet --repeat 2 --name Joe

However (obviously) these are not the same:

    : greet Joe 2
    : greet 2 Joe

(The second would be an error because 'Joe' is not a number).

Named arguments are assigned first, then the remaining arguments are assigned
to the remaining parameters. So the following is valid and unambiguous:

    : greet 2 --name Joe

Positional parameters quickly become unwieldy with long parameter lists so we
recommend only having 2 or 3 important parameters. GCLI provides hints for
important parameters more obviously than unimportant ones.

Parameters are 'important' if they are not in a parameter group. The easiest way
to achieve this is to use the ```option: true``` property.

For example, using:

    gcli.addItems([{
      name: 'greet',
      params: [
        { name: 'name', type: 'string' },
        { name: 'repeat', type: 'number', option: true, defaultValue: 1 }
      ],
      ...
    }]);

Would mean that this is an error

    : greet Joe 2

You would instead need to do the following:

    : greet Joe --repeat 2

For more on parameter groups, see below.

In addition to being 'important' and 'unimportant' parameters can also be
optional. If is possible to be important and optional, but it is not possible
to be unimportant and non-optional.

Parameters are optional if they either:
- Have a ```defaultValue``` property
- Are of ```type=boolean``` (boolean arguments automatically default to being false)

There is currently no way to make parameters mutually exclusive.


## Selection types

Parameters can have a type of ``selection``. For example:

    gcli.addItems([{
      name: 'greet',
      params: [
        { name: 'name', ... },
        {
          name: 'lang',
          description: 'In which language should we greet',
          type: { name: 'selection', data: [ 'en', 'fr', 'de', 'es', 'gk' ] },
          defaultValue: 'en'
        }
      ],
      ...
    }]);

GCLI will enforce that the value of ``arg.lang`` was one of the values
specified. Alternatively ``data`` can be a function which returns an array of
strings.

The ``data`` property is useful when the underlying type is a string but it
doesn't work when the underlying type is something else. For this use the
``lookup`` property as follows:

      type: {
        name: 'selection',
        lookup: {
          'en': Locale.EN,
          'fr': Locale.FR,
          ...
        }
      },

Similarly, ``lookup`` can be a function returning the data of this type.


## Number types

Number types are mostly self explanatory, they have one special property which
is the ability to specify upper and lower bounds for the number:

    gcli.addItems([{
      name: 'volume',
      params: [
        {
          name: 'vol',
          description: 'How loud should we go',
          type: { name: 'number', min: 0, max: 11 }
        }
      ],
      ...
    }]);

You can also specify a ``step`` property which specifies by what amount we
should increment and decrement the values. The ``min``, ``max``, and ``step``
properties are used by the command line when up and down are pressed and in
the input type of a dialog generated from this command.


## Delegate types

Delegate types are needed when the type of some parameter depends on the type
of another parameter. For example:

    : set height 100
    : set name "Joe Walker"

We can achieve this as follows:

    gcli.addItems([{
      name: 'set',
      params: [
        {
          name: 'setting',
          type: { name: 'selection', values: [ 'height', 'name' ] }
        },
        {
          name: 'value',
          type: {
            name: 'delegate',
            delegateType: function() { ... }
          }
        }
      ],
      ...
    }]);

Several details are left out of this example, like how the delegateType()
function knows what the current setting is. See the ``pref`` command for an
example.


## Array types

Parameters can have a type of ``array``. For example:

    gcli.addItems([{
      name: 'greet',
      params: [
        {
          name: 'names',
          type: { name: 'array', subtype: 'string' },
          description: 'The names to greet',
          defaultValue: [ 'World!' ]
        }
      ],
      ...
      exec: function(args, context) {
        return "Hello, " + args.names.join(', ') + '.';
      }
    }]);

This would be used as follows:

    : greet Fred Jim Shiela
    Hello, Fred, Jim, Shiela.

Or using named arguments:

    : greet --names Fred --names Jim --names Shiela
    Hello, Fred, Jim, Shiela.

There can only be one ungrouped parameter with an array type, and it must be
at the end of the list of parameters (i.e. just before any parameter groups).
This avoids confusion as to which parameter an argument should be assigned.


## Sub-commands

It is common for commands to be groups into those with similar functionality.
Examples include virtually all VCS commands, ``apt-get``, etc. There are many
examples of commands that should be structured as in a sub-command style -
``tar`` being the obvious example, but others include ``crontab``.

Groups of commands are specified with the top level command not having an
exec function:

    gcli.addItems([
      {
        name: 'tar',
        description: 'Commands to manipulate archives',
      },
      {
        name: 'tar create',
        description: 'Create a new archive',
        exec: function(args, context) { ... },
        ...
      },
      {
        name: 'tar extract',
        description: 'Extract from an archive',
        exec: function(args, context) { ... },
        ...
      }
    ]);


## Parameter groups

Parameters can be grouped into sections.

There are 3 ways to assign a parameter to a group.

The simplest uses ```option: true``` to put a parameter into the default
'Options' group:

    gcli.addItems([{
      name: 'greet',
      params: [
        { name: 'repeat', type: 'number', option: true }
      ],
      ...
    }]);

The ```option``` property can also take a string to use an alternative parameter
group:

    gcli.addItems([{
      name: 'greet',
      params: [
        { name: 'repeat', type: 'number', option: 'Advanced' }
      ],
      ...
    }]);

An example of how this can be useful is 'git' which categorizes parameters into
'porcelain' and 'plumbing'.

Finally, parameters can be grouped together as follows:

    gcli.addItems([{
      name: 'greet',
      params: [
        { name: 'name', type: 'string', description: 'The name to greet' },
        {
          group: 'Advanced Options',
          params: [
            { name: 'repeat', type: 'number', defaultValue: 1 },
            { name: 'debug', type: 'boolean' }
          ]
        }
      ],
      ...
    }]);

This could be used as follows:

    : greet Joe --repeat 2 --debug
    About to send greeting
    Hello, Joe
    Hello, Joe
    Done!

Parameter groups must come after non-grouped parameters because non-grouped
parameters can be assigned positionally, so their index is important. We don't
want 'holes' in the order caused by parameter groups.


## Command metadata

Each command should have the following properties:

- A string ``name``.
- A short ``description`` string. Generally no more than 20 characters without
  a terminating period/fullstop.
- A function to ``exec``ute. (Optional for the parent containing sub-commands)
  See below for more details.

And optionally the following extra properties:

- A declaration of the accepted ``params``.
- A ``hidden`` property to stop the command showing up in requests for help.
- A ``context`` property which defines the scope of the function that we're
  calling. Rather than simply call ``exec()``, we do ``exec.call(context)``.
- A ``manual`` property which allows a fuller description of the purpose of the
  command.
- A ``returnType`` specifying how we should handle the value returned from the
  exec function.

The ``params`` property is an array of objects, one for each parameter. Each
parameter object should have the following 3 properties:

- A string ``name``.
- A short string ``description`` as for the command.
- A ``type`` which refers to an existing Type (see Writing Types).

Optionally each parameter can have these properties:

- A ``defaultValue`` (which should be in the type specified in ``type``).
  The defaultValue will be used when there is no argument supplied for this
  parameter on the command line.
  If the parameter has a ``defaultValue``, other than ``undefined`` then the
  parameter is optional, and if unspecified on the command line, the matching
  argument will have this value when the function is called.
  If ``defaultValue`` is missing, or if it is set to ``undefined``, then the
  system will ensure that a value is provided before anything is executed.
  There are 2 special cases:
  - If the type is ``selection``, then defaultValue must not be undefined.
    The defaultValue must either be ``null`` (meaning that a value must be
    supplied by the user) or one of the selection values.
  - If the type is ``boolean``, then ``defaultValue:false`` is implied and
    can't be changed. Boolean toggles are assumed to be off by default, and
    should be named to match.
- A ``manual`` property for parameters is exactly analogous to the ``manual``
  property for commands - descriptive text that is longer than than 20
  characters.


## The Command Function (exec)

The parameters to the exec function are designed to be useful when you have a
large number of parameters, and to give direct access to the environment (if
used).

    gcli.addItems([{
      name: 'echo',
      description: 'The message to display.',
      params: [
        {
          name: 'message',
          type: 'string',
          description: 'The message to display.'
        }
      ],
      returnType: 'string',
      exec: function(args, context) {
        return args.message;
      }
    }]);

The ``args`` object contains the values specified on the params section and
provided on the command line. In this example it would contain the message for
display as ``args.message``.

The ``context`` object has the following signature:

    {
      environment: ..., // environment object passed to createTerminal()
      exec: ...,        // function to execute a command
      update: ...,      // function to alter the text of the input area
      createView: ...,  // function to help creating rich output
      defer: ...,       // function to create a deferred promise
    }

The ``environment`` object is opaque to GCLI. It can be used for providing
arbitrary data to your commands about their environment. It is most useful
when more than one command line exists on a page with similar commands in both
which should act in their own ways.
An example use for ``environment`` would be a page with several tabs, each
containing an editor with a command line. Commands executed in those editors
should apply to the relevant editor.
The ``environment`` object is passed to GCLI at startup (probably in the
``createTerminal()`` function).

The ``document`` object is also passed to GCLI at startup. In some environments
(e.g. embedded in Firefox) there is no global ``document``. This object
provides a way to create DOM nodes.

``defer()`` allows commands to execute asynchronously.


## Returning data

The command meta-data specifies the type of data returned by the command using
the ``returnValue`` setting.

``returnValue`` processing is currently functioning, but incomplete, and being
tracked in [Bug 657595](http://bugzil.la/657595). Currently you should specify
a ``returnType`` of ``string`` or ``html``. If using HTML, you can return
either an HTML string or a DOM node.

In the future, JSON will be strongly encouraged as the return type, with some
formatting functions to convert the JSON to HTML.

Asynchronous output is achieved using a promise created from the ``context``
parameter: ``context.defer()``.

Some examples of this is practice:

    { returnType: "string" }
    ...
    return "example";

GCLI interprets the output as a plain string. It will be escaped before display
and available as input to other commands as a plain string.

    { returnType: "html" }
    ...
    return "<p>Hello</p>";

GCLI will interpret this as HTML, and parse it for display.

    { returnType: "dom" }
    ...
    return util.createElement(context.document, 'div');

``util.createElement`` is a utility to ensure use of the XHTML namespace in XUL
and other XML documents. In an HTML document it's functionally equivalent to
``context.document.createElement('div')``. If your command is likely to be used
in Firefox or another XML environment, you should use it. You can import it
with ``var util = require('util/util');``.

GCLI will use the returned HTML element as returned. See notes on ``context``
above.

    { returnType: "number" }
    ...
    return 42;

GCLI will display the element in a similar way to a string, but it the value
will be available to future commands as a number.

    { returnType: "date" }
    ...
    return new Date();

    { returnType: "file" }
    ...
    return new File();

Both these examples return data as a given type, for which a converter will
be required before the value can be displayed. The type system is likely to
change before this is finalized. Please contact the author for more
information.

    { returnType: "string" }
    ...
    var deferred = context.defer();
    setTimeout(function() {
      deferred.resolve("hello");
    }, 500);
    return deferred.promise;

Errors can be signaled by throwing an exception. GCLI will display the message
property (or the toString() value if there is no message property). (However
see *3 principles for writing commands* above for ways to avoid doing this).


## Specifying Types

Types are generally specified by a simple string, e.g. ``'string'``. For most
types this is enough detail. There are a number of exceptions:

* Array types. We declare a parameter to be an array of things using ``[]``,
  for example: ``number[]``.
* Selection types. There are 3 ways to specify the options in a selection:
  * Using a lookup map

            type: {
              name: 'selection',
              lookup: { one:1, two:2, three:3 }
            }

    (The boolean type is effectively just a selection that uses
    ``lookup:{ 'true': true, 'false': false }``)

  * Using given strings

            type: {
              name: 'selection',
              data: [ 'left', 'center', 'right' ]
            }

  * Using named objects, (objects with a ``name`` property)

            type: {
              name: 'selection',
              data: [
                { name: 'Google', url: 'http://www.google.com/' },
                { name: 'Microsoft', url: 'http://www.microsoft.com/' },
                { name: 'Yahoo', url: 'http://www.yahoo.com/' }
              ]
            }

* Delegate type. It is generally best to inherit from Delegate in order to
  provide a customization of this type. See settingValue for an example.

See below for more information.
