<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# console #

The `console` object enables your add-on to log messages. If you have started
the host application for your add-on from the command line (for example, by
executing `cfx run` or `cfx test`) then these messages appear in the command
shell you used. If the add-on has been installed in the host application, then
the messages appear in the host application's
[Error Console](https://developer.mozilla.org/en/Error_Console).

The `console` object has the following methods:

<code>console.**log**(*object*[, *object*, ...])</code>

Logs an informational message to the shell.
Depending on the console's underlying implementation and user interface,
you may be able to introspect into the properties of non-primitive objects
that are logged.

<code>console.**info**(*object*[, *object*, ...])</code>

A synonym for `console.log()`.

<code>console.**warn**(*object*[, *object*, ...])</code>

Logs a warning message.

<code>console.**error**(*object*[, *object*, ...])</code>

Logs an error message.

<code>console.**debug**(*object*[, *object*, ...])</code>

Logs a debug message.

<code>console.**exception**(*exception*)</code>

Logs the given exception instance as an error, outputting information
about the exception's stack traceback if one is available.

<code>console.**trace**()</code>

Logs a stack trace at the point this function is called.
