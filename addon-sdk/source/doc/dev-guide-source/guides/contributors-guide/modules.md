<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

#Modules
A module is a self-contained unit of code, which is usually stored in a file,
and has a well defined interface. The use of modules greatly improves the
maintainability of code, by splitting it up into independent components, and
enforcing logical boundaries between them. Unfortunately, JavaScript does not
yet have native support for modules: it has to rely on the host application to
provide it with functionality such as loading subscripts, and exporting/
importing names. We will show how to do each of these things using the built-in
Components object provided by Xulrunner application such as Firefox and
Thunderbird.

To improve encapsulation, each module should be defined in the scope of its own
global object. This is made possible by the use of sandboxes. Each sandbox lives
in its own compartment. A compartment is a separate memory space. Each
compartment has a set of privileges that determines what scripts running in that
compartment can and cannot do. We will show how sandboxes and compartments can
be used to improve security in our module system.

The module system used by the SDK is based on the CommonJS specification: it is
implemented using a loader object, which handles all the bookkeeping related to
module loading, such as resolving and caching URLs. We show how to create your
own custom loaders, using the `Loader` constructor provided by the SDK. The SDK
uses its own internal loader, known as Cuddlefish. All modules within the SDK
are loaded using Cuddlefish by default. Like any other custom loader, Cuddlefish
is created using the `Loader` constructor. In the final section, we will take a
look at some of the options passed by the SDK to the `Loader` constructor to
create the Cuddlefish loader.

##Loading Subscripts
When a JavaScript project reaches a certain size, it becomes necessary to split
it up into multiple files. Unfortunately, JavaScript does not provide any means
to load scripts from other locations: we have to rely on the host application to
provide us with this functionality. Applications such as Firefox and Thunderbird
are based on Xulrunner. Xulrunner adds a built-in object, known as `Components`,
to the global scope. This object forms the central access point for all
functionality provided by the host application. A complete explanation of how to
use `Components` is out of scope for this document. However, the following
example shows how it can be used to load scripts from other locations: 

    const {
        classes: Cc
        interfaces: Ci
    } = Components;

    var instance = Cc["@mozilla.org/moz/jssubscript-loader;1"];
    var loader = instance.getService(Ci.mozIJSSubScriptLoader);

    function loadScript(url) {
        loader.loadSubScript(url);
    }

When a script is loaded, it is evaluated in the scope of the global object of
the script that loaded it. Any property defined on the global object will be
accessible from both scripts:

    index.js:
    loadScript("www.foo.com/a.js");
    foo; // => 3

    a.js:
    foo = 3;

##Exporting Names
The script loader we obtained from the `Components` object allows us load
scripts from other locations, but its API is rather limited. For instance, it
does not know how to handle relative URLs, which is cumbersome if you want to
organize your project hierarchically. A more serious problem with the
`loadScript` function, however, is that it evaluates all scripts in the scope of
the same global object. This becomes a problem when two scripts try to define
the same property:

    index.js:
    loadScript("www.foo.com/a.js");
    loadScript("www.foo.com/b.js");
    foo; // => 5

    a.js:
    foo = 3;

    b.js:
    foo = 5;

In the above example, the value of `foo` depends on the order in which the
subscripts are loaded: there is no way to access the property foo defined by
"a.js", since it is overwritten by "b.js". To prevent scripts from interfering
with each other, `loadScript` should evaluate each script to be loaded in the
scope of their own global object, and then return the global object as its
result. In effect, any properties defined by the script being loaded on its
global object are exported to the loading script. The script loader we obtained
from `Components` allows us to do just that:

    function loadScript(url) {
        let global = {};
        loader.loadSubScript(url, global);
        return global;
    }

If present, the `loadSubScript` function evaluates the script to be loaded in
the scope of the second argument. Using this new version of `loadScript`, we can
now rewrite our earlier example as follows

    index.js:
    let a = loadScript("www.foo.com/a.js");
    let b = loadScript("www.foo.com/b.js");

    a.foo // => 3
    b.foo; // => 5

    a.js:
    foo = 3;

    b.js:
    foo = 5;:

##Importing Names
In addition to exporting properties from the script being loaded to the loading
script, we can also import properties from the loading script to the script
being loaded:

    function loadScript(url, imports) {
        let global = {
            imports: imports,
            exports: {}
        };
        loader.loadSubScript(url, global);
        return global.exports;
    }

Among other things, this allows us to import `loadScript` to scripts being
loaded, allowing them to load further scripts:

    index.js:
    loadScript("www.foo.com/a.js", {
        loadScript: loadScript
    }).foo; => 5

    a.js:
    exports.foo = imports.loadScript("www.foo.com/b.js").bar;

    b.js:
    exports.bar = 5;

##Sandboxes and Compartments
The `loadScript` function as defined int the previous section still has some
serious shortcomings. The object it passed to the `loadSubScript` function is an
ordinary object, which has the global object of the loading script as its
prototype. This breaks encapsulation, as it allows the script being loaded to
access the built-in constructors of the loading script, which are defined on its
global object. The problem with breaking encapsulation like this is that
malicious scripts can use it to get the loading script to execute arbitrary
code, by overriding one of the methods on the built-in constructors. If the
loading script has chrome privileges, then so will any methods called by the
loading script, even if that method was installed by a malicious script.

To avoid problems like this, the object passed to `loadSubScript` should be a
true global object, having its own instances of the built-in constructors. This
is exactly what sandboxes are for. A sandbox is a global object that lives in a
separate compartment. Compartments are a fairly recent addition to SpiderMonkey,
and can be seen as a separate memory space. Objects living in one compartment
cannot be accessed directly from another compartment: they need to be accessed
through an intermediate object, known as a wrapper. Compartments are very
useful from a security point of view: each compartment has a set of privileges
that determines what a script running in that compartment can and cannot do.
Compartments with chrome privileges have access to the `Components` object,
giving them full access to the host platform. In contrast, compartments with
content privileges can only use those features available to ordinary websites.

The `Sandbox` constructor takes a `URL` parameter, which is used to determine
the set of privileges for the compartment in which the sandbox will be created.
Passing an XUL URL will result in a compartment with chrome privileges (note,
however, that if you ever actually do this in any of your code, Gabor will be
forced to hunt you down and kill you). Otherwise, the compartment will have
content privileges by default. Rewriting the `loadScript` function using
sandboxes, we end up with:

    function loadScript(url, imports) {
        let global = Components.utils.Sandbox(url);
        global.imports = imports;
        global.exports = {};
        loader.loadSubScript(url, global);
        return global.exports;
    }

Note that the object returned by `Sandbox` is a wrapper to the sandbox, not the
sandbox itself. A wrapper behaves exactly like the wrapped object, with one
difference: for each property access/function it performs an access check to
make sure that the calling script is actually allowed to access/call that
property/function. If the script being loaded is less privileged than the
loading script, the access is prevented, as the following example shows:

    index.js:
    let a = loadScript("www.foo.com/a.js", {
        Components: Components
    });

    // index.js has chrome privileges
    Components.utils; // => [object nsXPCComponents_Utils]

    a.js:
    // a.js has content privileges
    imports.Components.utils; // => undefined

##Modules in the Add-on SDK
The module system used by the SDK is based on what we learned so far: it follows
the CommonJS specification, which attempts to define a standardized module API.
A CommonJS module defines three global variables: `require`, which is a function
that behaves like `loadScript` in our examples, `exports`, which behaves
like the `exports` object, and `module`, which is an object representing
the module itself. The `require` function has some extra features not provided
by `loadScript`: it solves the problem of resolving relative URLs (which we have
left unresolved), and provides a caching mechanism, so that when the same module
is loaded twice, it returns the cached module object rather than triggering
another download. The module system is implemented using a loader object, which
is actually provided as a module itself. It is defined in the module
“toolkit/loader”:

    const { Loader } = require('toolkit/loader')

The `Loader` constructor allows you to create your own custom loader objects. It
takes a single argument, which is a named options object. For instance, the
option `paths` is used to specify a list of paths to be used by the loader to
resolve relative URLs:

    let loader = Loader({
        paths: ["./": http://www.foo.com/"]
    });

CommonJS also defines the notion of a main module. The main module is always the
first to be loaded, and differs from ordinary modules in two respects. Firstly,
since they do not have a requiring module. Instead, the main module is loaded
using a special function, called `main`:

    const { Loader, main } = require('toolkit/loader');

    let loader = Loader({
        paths: ["./": http://www.foo.com/"]
    });

    main(loader, "./main.js");

Secondly, the main module is defined as a property on `require`. This allows
modules to check if it they have been loaded as the main module:

    function main() {
        ...
    }

    if (require.main === module)
        main();

##The Cuddlefish Loader
The SDK uses its own internal loader, known as Cuddlefish (because we like crazy
names). Like any other custom loader, Cuddlefish is created using the `Loader`
constructor: Let's take a look at some of the options used by Cuddlefish to
customize its behavior. The way module ids are resolved can be customized by
passing a custom `resolve` function as an option. This function takes the id to
be resolved and the requiring module as an argument, and returns the resolved id
as its result. The resolved id is then further resolved using the paths array:

    const { Loader, main } = require('toolkit/loader');

    let loader = Loader({
        paths: ["./": "http://www.foo.com/"],
        resolve: function (id, requirer) {
            // Your code here
            return id;
        }
    });
    main(loader, "./main.js");

Cuddlefish uses a custom `resolve` function to implement a form of access
control: modules can only require modules for which they have been explicitly
granted access. A whitelist of modules is generated statically when the add-on
is linked. It is possible to pass a list of predefined modules as an option to
the `Loader` constructor. This is useful if the API to be exposed does not have
a corresponding JS file, or is written in an incompatible format. Cuddlefish
uses this option to expose the `Components` object as a module called `chrome`,
in a way similar to the code here below:

    const {
        classes: Cc,
        Constructor: CC,
        interfaces: Ci,
        utils: Cu,
        results: Cr,
        manager: Cm
    } = Components;

    let loader = Loader({
        paths: ["./": "http://www.foo.com/"],
        resolve: function (id, requirer) {
            // Your logic here
            return id;
        },
        modules: {
            'chrome': {
                components: Components,
                Cc: Cc,
                CC: bind(CC, Components),
                Ci: Ci,
                Cu: Cu,
                Cr: Cr,
                Cm: Cm
            }
        }
    });

All accesses to the `chrome` module go through this one point. As a result, we
don't have to give modules chrome privileges on a case by case basis. More
importantly, however, any module that wants access to `Components` has to
explicitly express its intent via a call to `require("chrome")`. This makes it
possible to reason about which modules have chrome capabilities and which don't.
