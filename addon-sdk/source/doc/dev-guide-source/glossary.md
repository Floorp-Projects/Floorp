<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Glossary #

This glossary contains a list of terms used in the Add-on SDK.

__Add-on__: A software package that adds functionality to a Mozilla application,
which can be built with either Mozilla's traditional add-on platform or the SDK.

__Add-on SDK__: A toolchain and associated applications for developing add-ons.

__API Utils__: A small, self-contained set of low-level modules that forms
the base functionality for the SDK. The library can be "bootstrapped" into
any Mozilla application or add-on.

__CFX__: A command-line build, testing, and packaging tool for SDK-based code.

__CommonJS__: A specification for a cross-platform JavaScript module
system and standard library.  [Web site](http://commonjs.org/).

__Extension__: Synonym for Add-on.

__Globals__: The set of global variables and objects provided
to all modules, such as `console` and `memory`. Includes
CommonJS globals like `require` and standard JavaScript globals such
as `Array` and `Math`.

<span><a name="host-application">__Host Application__:</a> Add-ons are executed in
the context of a host application, which is the application they are extending.
Firefox and Thunderbird are the most obvious hosts for Mozilla add-ons, but
at present only Firefox is supported as a host for add-ons developed using the
Add-on SDK.</span>

__Jetpack Prototype__: A Mozilla Labs experiment that predated and inspired
the SDK. The SDK incorporates many ideas and some code from the prototype.

__Loader__: An object capable of finding, evaluating, and
exposing CommonJS modules to each other in a given security context,
while providing each module with necessary globals and
enforcing security boundaries between the modules as necessary. It's
entirely possible for Loaders to create new Loaders.

__Low-Level Module__: A module with the following properties:

  * Has "chrome" access to the Mozilla platform (e.g. `Components.classes`)
    and all globals.
  * Is reloadable without leaking memory.
  * Logs full exception tracebacks originating from client-provided
    callbacks (i.e., does not allow the exceptions to propagate into
    Mozilla platform code).
  * Can exist side-by-side with multiple instances and versions of
    itself.
  * Contains documentation on security concerns and threat modeling.

__Module__: A CommonJS module that is either a Low-Level Module
or an Unprivileged Module.

__Package__: A directory structure containing modules,
documentation, tests, and related metadata. If a package contains
a program and includes proper metadata, it can be built into
a Mozilla application or add-on.

__Program__: A module named `main` that optionally exports
a `main()` function.  This module is intended either to start an application for
an end-user or add features to an existing application.

__Unprivileged Module__: A CommonJS module that may be run
without unrestricted access to the Mozilla platform, and which may use
all applicable globals that don't require chrome privileges.

  [Low-Level Module Best Practices]: dev-guide/module-development/best-practices.html
