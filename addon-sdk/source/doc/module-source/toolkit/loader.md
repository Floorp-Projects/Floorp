<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `loader` module exposes a low level API for creating
[CommonJS module][CommonJS Modules]
loaders. The code is intentionally authored so that it
can be loaded in several ways:

1. It can be loaded as a regular script tag in documents that have
[system principals][]:

    <pre class="brush: html">
        &lt;script type='application/javascript' src='resource://gre/modules/toolkit/loader.js'&gt;&lt;/script&gt;
    </pre>

    This will expose a single `loader` object containing all of the
    API functions described in this document.

2. It can be loaded as a [JavaScript code module][]:

        let { Loader, Require, unload } = Components.utils.import('resource://gre/modules/toolkit/loader.js');

3. It can be required as a CommonJS module from a module loaded in
the loader itself:

        let { Loader, Require, unload } = require('toolkit/loader');

## What is it good for ?

- Create somewhat standardized JS environments that people doing
non-browser JS are familiar with.
- Provide an environment for loading
  [CommonJS style modules][CommonJS modules], which makes it possible
  to consume [lots of interesting code](http://search.npmjs.org/) that
  has already been developed.
- Secure each module in an isolated JS sandbox and makes any
  capability imports explicit via calls to the `require()` function.
- create task specific loaders with restricted module access.
- Provide unload hooks that may be used to undo changes made by
  anything loaded into it.

## Instantiation

The `loader` module provides a `Loader()` function that may be used
to instantiate new loader instances:

    let loader = Loader(options);

## Configuration

`Loader()` may be provided with a set of configuration options:

* [`paths`](modules/toolkit/loader.html#paths): describes where the loader should
find the modules it is asked to load. **Mandatory**.
* [`modules`](modules/toolkit/loader.html#modules): provides a set of module exports. **Optional**.
* [`globals`](modules/toolkit/loader.html#globals): provides a set of globals shared across modules loaded
via this loader. **Optional**.
* [`resolve`](modules/toolkit/loader.html#resolve): provide customized module resolution logic. **Optional**.
* [`id`](modules/toolkit/loader.html#id): provide add-on id to attach to loaded
modules. **Optional**.

### paths

The loader needs to be provided with a set of locations to indicate
where to find modules loaded using `require()`. This is provided by
a mandatory `options.paths` hash that represents the mapping between
module ID prefixes and locations. There are lots of
different possibilities, but the most common setup looks like this:

    let { Loader } = require('toolkit/loader');
    let loader = Loader({
      paths: {
        // Resolve all modules starting with `toolkit/` as follows:
        // toolkit/foo      ->  resource://gre/modules/toolkit/foo.js
        // toolkit/foo/bar  ->  resource://gre/modules/toolkit/foo/bar.js
        'toolkit/': 'resource://gre/modules/toolkit/',
        // Resolve all other non-relative module requirements as follows:
        // devtools/gcli    ->  resource:///modules/devtools/gcli.js
        // panel            ->  resource:///modules/panel.js
        '': 'resource:///modules/',
      }
    })

All relative URL `require()` statements (those that start with ".")
are first resolved relative to the requirer module ID and the result
of it is then resolved using the `paths` option. You may
still end up with a relative module ID if the entry point module ID is
itself relative. In those cases you have to decide what the entry point
module is relative to and provide an appropriate mapping for it:

    let { Loader } = require('toolkit/loader');
    let loader = Loader({
      paths: {
        // Resolve all modules starting with `toolkit/` as follows:
        // toolkit/foo      ->  resource://gre/modules/toolkit/foo.js
        // toolkit/foo/bar  ->  resource://gre/modules/toolkit/foo/bar.js
        'toolkit/': 'resource://gre/modules/toolkit/',
        // Resolev all other non-relative module requirements as follows:
        // devtools/gcli    ->  resource:///modules/devtools/gcli.js
        // panel            ->  resource:///modules/panel.js
        '': 'resource:///modules/',
        // Allow relative URLs and resolve them to add-on root:
        // ./main           ->  resource://my-addon/root/main.js
        './': 'resource://my-addon/root/'
      }
    });

The order of keys in `paths` is irrelevant since they are sorted
by keys from longest to shortest to allow overlapping mapping.
The example above overlays the base path `''` with a different mapping
for `'toolkit/'` prefixed modules.

### modules

The loader may **optionally** be provided with a set of module exports.
In the SDK we call these "pseudo modules".
This feature may be used in a few different ways:

1. To expose an API that doesn't have a JS file with an implementation
   or is written in an incompatible format such as
   [JSM][JavaScript code module]:
   
        let { Loader } = require('toolkit/loader');
        let loader = Loader({
          modules: {
            // require('net/utils') will get NetUtil.jsm
            'net/utils': Cu.import('resource:///modules/NetUtil.jsm', {})
          }
        });
   Each loader instance comes with a set of built-in pseudo modules
   that are described in detail in the [Built-in Modules](modules/toolkit/loader.html#Built-in_Modules)  section.

2. To reuse module instances that are already loaded.
   For example in the SDK, the loader is loaded at bootstrap as a JSM module
   but is then exposed as a pseudo-module to avoid the overhead of subsequent
   loads:

        let loaderModule = Cu.import('resource://gre/modules/toolkit/loader.js');
        let loader = loaderModule.Loader({
          modules: {
            // Overlay `toolkit/loader` so that `require('toolkit/loader')`
            // will return our `loaderModule`.
            'toolkit/loader': loaderModule
          }
        });

   Use this feature with a great care though. While reuse may sound like a
   compelling idea it comes with the side effect of shared state, which can
   cause problems. For example, unload of a loader won't trigger
   unload hooks on pseudo-modules.

### globals

Each module loaded via the loader instance is secured in own JS sandbox.
These modules don't share scope and get their own set of built-ins
(`Object`, `Array`, `String` ...). But sometimes it's convenient to
define a set of common globals that will be shared across modules. This can be
done using the optional `globals` option.

For example, the SDK uses this feature to provide a global `console` object:

    let { Loader } = require('toolkit/loader');
    let loader = Loader({
      globals: {
        console: console: {
          log: dump.bind(dump, 'log: '),
          info: dump.bind(dump, 'info: '),
          warn: dump.bind(dump, 'warn: '),
          error: dump.bind(dump, 'error: ')
        }
      }
    });

Be careful not to misuse this feature! In general it is not recommended
to provide features via globals, it's almost always better to use
pseudo-modules or, even better, modules.

### resolve


The optional `resolve` option enables you to completely customize
module resolution logic.

`resolve` is assigned a function that takes:

* the ID of the module passed into `require()`
* the ID of the module that called `require()`

On each `require()` call, the supplied function is then called with
the ID of the required module and that of the requiring module.

The function returns a string representing the resolved module ID, which
is then resolved to its location URL using the mapping provided in
the `paths` option.

If this option is not provided, the loader will use plain path resolution. 

This feature may also be used to implement specific security constraints.
For example, the SDK generates a manifest file at build time representing
a dependency graph of all modules used by an add-on. Any attempt to load
a module not listed in the manifest is unauthorized and is rejected with an
exception:

    let { Loader } = require('toolkit/loader');
    let manifest = {
      './main': {
        'requirements': {
          'panel': 'sdk/panel'
        }
      },
      'sdk/panel': {
        'requirements': {
          'chrome': 'chrome'
        }
      }
      'chrome': {
        'requirements': {}
      }
    };
    let loader = Loader({
      resolve: function(id, requirer) {
        let requirements = manifest[requirer].requirements;
        if (id in manifest)
          return requirements[id];
        else
          throw Error('Module "' + requirer + '" has no authority to require ' +
                      'module "' + id + "')
      }
    });

Thrown exceptions will propagate to the caller of `require()`. If
the function assigned to `resolve`
does not return a string value, an exception will still be thrown as
the loader will be unable to resolve the required module's location.

### id

Add-on debugging requires knowing which objects belong to which add-on.
When created with this option, Loader will transparently mark all new global
objects with the provided value.

### All Together

All of these options can be combined to configure the loader for
a specific use case. Don't get too excited about configuration options, keep
in mind that modules are more useful if they can be used across
loader instances. Unless you have specific needs it's best to stick to an
SDK-compatible configuration, like this:

    let { Loader } = require('toolkit/loader');
    let loader = Loader({
      // Please note: Illustrated `paths` is expected to be a default base,
      // but depending on your use case you may have more mappings.
      paths: {
        // Resolve all non-relative module requirements to
        // `resource:///modules/` base URI.
        '': 'resource:///modules/',

        // Reserve `toolkit/` prefix for generic, mozilla toolkit modules
        // and resolve them to `resource://gre/modules/toolkit/` URI.
        'toolkit/': 'resource://gre/modules/toolkit/'
      },
      // Please note: Both `globals` and `modules` are just for illustration
      // purposes we don't suggest populating them with these values.
      globals: {
        // Provide developers with well known `console` object, hopefully
        // with a more advanced implementation.
        console: {
          log: dump.bind(dump, "log: "),
          info: dump.bind(dump, "info: "),
          warn: dump.bind(dump, "warn: "),
          error: dump.bind(dump, "error: ")
        }
      },
      modules: {
        // Expose legacy API via pseudo modules that eventually may be
        // replaced with a real ones :)
        "devtools/gcli": Cu.import("resource:///modules/gcli.jsm", {}),
        "net/utils": Cu.import("resource:///modules/NetUtil.jsm", {})
      }
    });

### Loader Instances

The loader produces instances that are nothing more than representations
of the environment into which modules are loaded. It is intentionally made
immutable and all its properties are just an implementation detail that
no one should depend on, they may change at any point without any further
notice.

## Loading Modules

The CommonJS specification defines the notion of a **main** module, which
represents an entry point to a program. 

The `loader` module exposes a `main()` function that can
be used to load a main module.
All other modules will be loaded by this module or its dependencies:

    let { main, Loader } = require('toolkit/loader');
    let loader = Loader(options);
    let program = main(loader, './main');

A module can find out whether it was loaded as main:

    if (require.main === module)
      main();

It is possible to load other modules before a main one, but it's inherently
harder to do. That's because every module except main has a requirer,
based on which resolution and authority decisions are made. In order to load
a module before a main one (for example to bootstrap an environment) the
requirer must be created first:

    let { Require, Loader, Module } = require('toolkit/loader');
    let loader = Loader(options);
    let requirer = Module(requirerID, requirerURI);
    let require = Require(loader, requirer);
    let boostrap = require(bootstrapID);

## Built-in Modules

Each loader instance exposes the following built-in pseudo modules
in addition to those passed via `modules`:

### chrome

This pseudo module exposes everything that is typically available for JS
contexts with [system principals][] under the [Components][] global.
This alternative approach of providing capabilities via modules makes
it possible to build module capability graphs by analyzing require
statements. These graphs can be used to reason about modules without diving
into implementation details.

### @loader/options

This pseudo module exposes all the `options` that were used to configure this
loader. It enables you to create new loader instances identical to the
current one:

        let { Loader } = require('toolkit/loader');
        let options = require('@loader/options');
        let loader = Loader(options);

This module is useful in very specific cases. For example the SDK uses this
feature during test execution to create an identical environment with a
different state to test how specific modules handle unloads.

### @loader/unload

This pseudo module exposes an object that is unique per loader
instance. It is used as a subject for [observer notification][] to allow
use of the [observer service][] for defining hooks reacting on the unload of
a specific loader.
The SDK builds a higher level API on top of this for handling unloads and
performing cleanup:

        let unloadSubject = require('@loader/unload');
        let observerService = Cc['@mozilla.org/observer-service;1'].
                              getService(Ci.nsIObserverService);

        let observer = {
          observe: function onunload(subject, topic, data) {
            // If this loader is unload then `subject.wrappedJSObject` will be
            // `unloadSubject`. `topic` is `'sdk:loader:destroy'`. `data` is
            // string describing unload reason.
            let unloadReason = data;
            if (subject.wrappedJSObject === unloadSubject)
              cleanup(reason)
          }
        };
        observerService.addObserver(observer, 'sdk:loader:destroy', false);

## Unload

The `loader` module exposes an `unload()` function that can be used to
unload specific loader instance and undo changes made by modules loaded
into it. `unload` takes `loader` as a first argument and optionally a
`reason` string identifying the reason why this loader was unloaded.

For example in the SDK `reason` may be one of:
`shutdown`, `disable`, `uninstall`.


        unload(loader, 'disable');

Calls to this function will dispatch the unload
[observer notification][] that modules can listen to as described above.

## Other Utilities

The loader module exposes several other utility functions that are used
internally and can also be handy while bootstrapping the loader itself.
They are low level helpers and should be used only during
loader bootstrap.

### Module()

The `Module()` function takes a module ID and URI and creates a module
instance object that is exposed as the `module` variable in the
module scope.

    let module  = Module('foo/bar', 'resource:///modules/foo/bar.js');

Note that this won't actually load any module code, it just creates
a placeholder for it. The section below describes how to load the module.

### load()

The `load()` function takes `loader` and loads the given `module` into it:

    let loader = Loader(options);
    let module  = Module('foo/bar', 'resource:///modules/foo/bar.js');
    load(loader, module);

### Sandbox()

The `Sandbox()` function is a utility function for creating
[JS sandboxes][]. It is used by the loader to create scopes into which
modules are loaded. It takes the following set of configuration options:

- `name`: A string value which identifies the sandbox in about:memory.
  Will throw exception if omitted.
- `principal`: String URI or `nsIPrincipal` for the sandbox.
  If omitted defaults to system principal.
- `prototype`: Object that the returned sandbox will inherit from.
   Defaults to `{}`.
- `wantXrays`: A Boolean value indicating whether code outside the sandbox
   wants X-ray vision with respect to objects inside the sandbox. Defaults
   to `true`.

<!-- this comment is used to terminate the Markdown list -->

       let sandbox = Sandbox({
         name: 'resource:///modules/foo/bar.js',
         wantXrays: false,
         prototype: {
           console: {
             log: dump.bind(dump, 'log: '),
             info: dump.bind(dump, 'info: '),
             warn: dump.bind(dump, 'warn: '),
             error: dump.bind(dump, 'error: ')
           }
         }
       });

### evaluate()

Evaluates code in the supplied `sandbox`.
If `options.source` is provided then its value is evaluated, otherwise
source is read from the supplied `uri`. Either way, any exceptions will
be reported as from the `uri`.

Optionally more options may be specified:

- `options.encoding`: source encoding, defaults to 'UTF-8'.
- `options.line`: line number to start count from in stack traces.
  Defaults to 1.
- `options.version`: version of JS used, defaults to '1.8'.

<!-- this comment is used to terminate the Markdown list -->

    // Load script from the given location to a given sandbox:
    evaluate(sandbox, 'resource://path/to/script.js')

    // Evaluate `code` as if it was from `foo/bar.js`:
    evaluate(sandbox, 'foo/bar.js', {
      source: code,
      version: '1.7'
      // You could also use other options described above.
    })

### Require()

As already mentioned in
[Loading Modules](modules/toolkit/loader.html#Loading_Modules)
it's common to start execution by loading a main module.
But sometimes you may want to prepare the environment using
existing modules before doing that. In such cases you can create a
requirer module instance and a version of `require` exposed to it with
this function:

    let requirer = Module(requirerID, requirerURI);
    let require = Require(loader, requirer);
    let boostrap = require(bootstrapID);

### resolveURI()

This function is used by the loader to resolve module URI from an ID using
a mapping array generated from the loader's `paths` option.
It examines each element until it finds the prefix matching the supplied
ID and replaces it with the location it maps to:

    let mapping = [
      [ 'toolkit/', 'resource://gre/modules/toolkit/' ],
      [ './',       'resource://my-addon/'    ], 
      [ '',         'resource:///modules/'    ]
    ];
    resolveURI('./main', mapping);           // => resource://my-addon/main.js
    resolveURI('devtools/gcli', mapping);    // => resource:///modules/devtools/gcli.js
    resolveURI('toolkit/promise', mapping);  // => resource://gre/modules/toolkit/promise.js

### override()

This function is used to create a fresh object that contains own properties of
two arguments it takes. If arguments have properties with conflicting names
the property from the second argument overrides that from the first. This
function is helpful for combining default and passed properties:

    override({ a: 1, b: 1 }, { b: 2, c: 2 }) // => { a: 1, b: 2, c: 2 }

### descriptor()

This utility function takes an object and as an argument and returns property
descriptor map for its own properties. It is useful when working with object
functions introduced in ES5 (`Object.create, Object.defineProperties, ..`):

    // define properties of `source` on `target`.
    Object.defineProperties(target, descriptor(source))

[CommonJS Modules]:http://wiki.commonjs.org/wiki/Modules/1.1.1
[system principals]:https://developer.mozilla.org/en/Security_check_basics#Principals
[JavaScript code module]:https://developer.mozilla.org/en/JavaScript_code_modules/Using
[Observer notification]:https://developer.mozilla.org/en/Observer_Notifications
[observer service]:https://developer.mozilla.org/en/nsiobserverservice
[Components]:https://developer.mozilla.org/en/Components
[JS Sandboxes]:https://developer.mozilla.org/en/Components.utils.Sandbox
