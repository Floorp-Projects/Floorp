<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<span class="aside">
If all you need to do is use XPCOM objects that someone else has implemented,
then you don't need to use this module. You can just use `require("chrome")`
to get direct access to the
[`Components`](https://developer.mozilla.org/en/Components_object)
object, and
[access XPCOM objects from there](dev-guide/guides/xul-migration.html#xpcom).
</span>

The `xpcom` module makes it simpler to perform three main tasks:

* [Implement XPCOM object interfaces](modules/sdk/platform/xpcom.html#Implementing XPCOM Interfaces)
* [Implement and register XPCOM factories](modules/sdk/platform/xpcom.html#Implementing XPCOM Factories)
* [Implement and register XPCOM services](modules/sdk/platform/xpcom.html#Implementing XPCOM Services)

## Implementing XPCOM Interfaces ##

<span class="aside">"`Unknown`" is named after the
["`IUnknown`"](http://msdn.microsoft.com/en-us/library/windows/desktop/ms680509%28v=vs.85%29.aspx) interface in COM.</span>

This module exports a class called `Unknown` which implements the fundamental
XPCOM interface
[`nsISupports`](https://developer.mozilla.org/docs/XPCOM_Interface_Reference/nsISupports).
By subclassing `Unknown`, either using
[standard JavaScript inheritance](https://developer.mozilla.org/en-US/docs/JavaScript/Guide/Inheritance_Revisited)
or using the SDK's [`heritage`](modules/sdk/core/heritage.html)
module, you can provide your own implementations of XPCOM interfaces.

For example, the add-on below implements the
[`nsIObserver`](https://developer.mozilla.org/docs/XPCOM_Interface_Reference/nsIObserver)
interface to listen for and log all topic notifications:

    var { Class } = require('sdk/core/heritage');
    var { Unknown } = require('sdk/platform/xpcom');
    var { Cc, Ci } = require('chrome')
    var observerService = Cc['@mozilla.org/observer-service;1'].
                            getService(Ci.nsIObserverService);

    var StarObserver = Class({
      extends:  Unknown,
      interfaces: [ 'nsIObserver' ],
      topic: '*',
      register: function register() {
        observerService.addObserver(this, this.topic, false);
      },
      unregister: function() {
        addObserver.removeObserver(this, this.topic);
      },
      observe: function observe(subject, topic, data) {
        console.log('star observer:', subject, topic, data);
      }
    });

    var starobserver = StarObserver();
    starobserver.register();

## Implementing XPCOM Factories ##

The `xpcom` module exports a class called `Factory` which implements the
[`nsIFactory`](https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIFactory)
interface. You can use this class to register factories for XPCOM
components you have defined.

For example, this add-on defines a subclass of `Unknown` called
`HelloWorld` that implements a function called `hello`. By
creating a `Factory` and passing it the
[contract ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#Contract_ID)
for the `HelloWorld` component and the `HelloWorld` constructor, we enable
XPCOM clients to access the `HelloWorld` component, given its contract ID.

<span class="aside">
In this example the `HelloWorld` component is available to JavaScript only,
so we use the technique documented under the "Using wrappedJSObject"
section of
[How to Build an XPCOM Component in JavaScript](https://developer.mozilla.org/en/How_to_Build_an_XPCOM_Component_in_Javascript).</span>

    var { Class } = require('sdk/core/heritage');
    var { Unknown, Factory } = require('sdk/platform/xpcom');
    var { Cc, Ci } = require('chrome');

    var contractId = '@me.org/helloworld';

    // Define a component
    var HelloWorld = Class({
      extends: Unknown,
      get wrappedJSObject() this,
      hello: function() {return 'Hello World';}
    });

    // Create and register the factory
    var factory = Factory({
      contract: contractId,
      Component: HelloWorld
    });

    // XPCOM clients can retrieve and use this new
    // component in the normal way
    var wrapper = Cc[contractId].createInstance(Ci.nsISupports);
    var helloWorld = wrapper.wrappedJSObject;
    console.log(helloWorld.hello());

### Using class ID ###

You can specify a
[class ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#CID)
for the factory by setting the `id` option in the factory's constructor.
If you don't specify a class ID, then the factory will generate one.
Either way, it will be accessible as the value of the factory's `id` property.

XPCOM users can look up the factory using the class ID instead of the
contract ID. Here's the example above, rewritten to use class ID
instead of contract ID for lookup:

    var { Class } = require('sdk/core/heritage');
    var { Unknown, Factory } = require('sdk/platform/xpcom');
    var { Cc, Ci, components } = require('chrome');

    // Define a component
    var HelloWorld = Class({
      extends: Unknown,
      get wrappedJSObject() this,
      hello: function() {return 'Hello World';}
    });

    // Create and register the factory
    var factory = Factory({
      Component: HelloWorld
    });

    var id = factory.id;

    // Retrieve the factory by class ID
    var wrapper = components.classesByID[id].createInstance(Ci.nsISupports);
    var helloWorld = wrapper.wrappedJSObject;
    console.log(helloWorld.hello());

### Replacing Factories ###

If the factory you create has the same contract ID as an existing registered
factory, then your factory will replace the existing one. However, the
`Components.classes` object commonly used to look up factories by contract ID
will not be updated at run time. To access the replacement factory you need
to do something like this:

    var id = Components.manager.QueryInterface(Ci.nsIComponentRegistrar).
      contractIDToCID('@me.org/helloworld');
    var wrapper = Components.classesByID[id].createInstance(Ci.nsISupports);

The `xpcom` module exports a function `factoryByContract` to simplify this
technique:

    var wrapper = xpcom.factoryByContract('@me.org/helloworld').createInstance(Ci.nsISupports);

### Registration ###

By default, factories are registered and unregistered automatically.
To learn more about this, see
[Registering and Unregistering](modules/sdk/platform/xpcom.html#Registering and Unregistering).

## Implementing XPCOM Services ##

The `xpcom` module exports a class called `Service` which you can use to
define
[XPCOM services](https://developer.mozilla.org/en-US/docs/XUL_Tutorial/XPCOM_Interfaces#XPCOM_Services),
making them available to all XPCOM users.

This example implements a logging service that just appends a timestamp
to all logged messages. The logger itself is implemented by subclassing
the `Unknown` class, then we create a service which associates the logger's
constructor with its
[contract ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#Contract_ID).
After this, XPCOM users can access the service using the `getService()` API:

    var { Class } = require('sdk/core/heritage');
    var { Unknown, Service } = require('sdk/platform/xpcom');
    var { Cc, Ci } = require('chrome');

    var contractId = '@me.org/timestampedlogger';

    // Implement the service by subclassing Unknown
    var TimeStampedLogger = Class({
      extends: Unknown,
      get wrappedJSObject() this,
      log: function(message) {
        console.log(new Date().getTime() + ' : ' + message);
      }
    });

    // Register the service using the contract ID
    var service = Service({
      contract: contractId,
      Component: TimeStampedLogger
    });

    // Access the service using getService()
    var wrapper = Cc[contractId].getService(Ci.nsISupports);
    var logger = wrapper.wrappedJSObject;
    logger.log('a timestamped message');

By default, services are registered and unregistered automatically.
To learn more about this, see
[Registering and Unregistering](modules/sdk/platform/xpcom.html#Registering and Unregistering).

## Registering and Unregistering ##

By default, factories and services are registered with XPCOM automatically
when they are created, and unregistered automatically when the add-on
that created them is unloaded.

You can override this behavior using the `register` and `unregister`
options to the factory or service constructor:

    var xpcom = require('sdk/platform/xpcom');

    var factory = xpcom.Factory({
      contract: contractId,
      Component: HelloWorld,
      register: false,
      unregister: false,
    });

If you disable automatic registration in this way, you can use the
`register()` function to register factories and services:

    xpcom.register(factory);

You can use the corresponding `unregister()` function to unregister them,
whether or not you have disabled automatic unregistration:

    xpcom.unregister(factory);


You can find out whether a factory or service has been registered by using the
`isRegistered()` function:

    if (xpcom.isRegistered(factory))
      xpcom.unregister(factory);

<api name="Unknown">
@class
This is the base class for all XPCOM objects. It is not intended to be used
directly but you can subclass it,
either using
[standard JavaScript inheritance](https://developer.mozilla.org/en-US/docs/JavaScript/Guide/Inheritance_Revisited)
or using the SDK's [`heritage`](modules/sdk/core/heritage.html)
module, to create new
implementations of XPCOM interfaces. For example, this subclass implements the
[`nsIRequest`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/NsIRequest)
interface:

    var { Class } = require('sdk/core/heritage');
    var { Unknown } = require('sdk/platform/xpcom');

    var Request = Class({
      extends: Unknown,
      interfaces: [ 'nsIRequest' ],
      initialize: function initialize() {
        this.pending = false;
      },
      isPending: function() { return this.pending; },
      resume: function() { console.log('resuming...'); },
      suspend: function() { console.log('suspending...'); },
      cancel: function() { console.log('canceling...'); }
    });

This component definition:

<span class="aside">Although `Request` also implements
`nsISupports`, there is no need to add it here, because
the base class `Unknown` declares support for `nsISupports` and this
is accounted for when retrieving objects.</span>

* specifies that we support `nsIRequest` using the `interfaces` property.
* initializes `pending` in `initialize()`
* adds our implementation of the `nsIRequest` interface

We can register a factory for this component by using the `Factory`
class to associate its constructor with its contract ID:

    var { Factory } = require('sdk/platform/xpcom');
    var { Cc, Ci } = require('chrome');
    var contractId = '@me.org/request'

    // Create and register the factory
    var factory = Factory({
      contract: contractId,
      Component: Request
    });

Now XPCOM users can access our implementation in the normal way:

    var request = Cc[contractId].createInstance(Ci.nsIRequest);
    request.resume();

<api name="QueryInterface">
@method
This method is called automatically by XPCOM, so usually you don't
need to call it yourself. It is passed an interface identifier and
searches for the identifier in the `interfaces` property of:

* this object
* any of this object's ancestors
* any classes in the `implements` array property of the instance
(for example, any classes added to this object via the `implements`
option defined in [`heritage`](modules/sdk/core/heritage.html)).

If it finds a match, it returns `this`, otherwise it throws
[`Components.results.NS_ERROR_NO_INTERFACE`](https://developer.mozilla.org/en-US/docs/Table_Of_Errors).

@param interface {iid}
The interface to ask for. This is typically given as a property of the
[`Components.interfaces`](https://developer.mozilla.org/en-US/docs/Components.interfaces)
object.

@returns {object}
The object itself(`this`).
</api>

<api name="interfaces">
@property {array}
The set of interfaces supported by this class. `Unknown` sets this to
[`nsISupports`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsISupports).
</api>

</api>

<api name="Factory">
@class
Use this class to register an XPCOM factory. To register a factory for
a component, construct a `Factory`, giving it:

* a constructor for the component
* a [contract ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#Contract_ID)
and/or a
[class ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#CID)

<!-- -->

    // Create and register the factory
    var factory = Factory({
      contract: '@me.org/myComponent',
      Component: MyComponent
    });

* The component constructor typically returns a descendant of `Unknown`,
although it may return a custom object that satisfies the `nsISupports`
interface.
* The contract ID and/or class ID may be used by XPCOM clients to retrieve
the factory. If a class ID is not given, a new one will be generated. The
contract ID and class ID are accessible as the values of the `contract` and
`id` properties, respectively.
* By default, the factory is registered when it is created and
unregistered when the add-on that created it is unloaded. To override this
behavior, you can pass `register` and/or `unregister` options, set to `false`.
If you do this, you can use the
[`register()`](modules/sdk/platform/xpcom.html#register(factory)) and
[`unregister()`](modules/sdk/platform/xpcom.html#unregister(factory)) functions
to register and unregister.

<api name="Factory">
@constructor

@param options {object}

@prop Component {constructor}
Constructor for the component this factory creates. This will typically
return a descendant of `Unknown`, although it may return a custom object
that satisfies the `nsISupports` interface.

@prop [contract] {string}
A [contract ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#Contract_ID).
Users of XPCOM can use this value to retrieve the factory from
[`Components.classes`](https://developer.mozilla.org/en-US/docs/Components.classes):

    var factory = Components.classes['@me.org/request']
    var component = factory.createInstance(Ci.nsIRequest);

This parameter is formally optional, but if you don't specify it, users
won't be able to retrieve your factory using a contract ID. If specified,
the contract ID is accessible as the value of the factory's `contract`
property.

@prop [id] {string}
A [class ID](https://developer.mozilla.org/en-US/docs/Creating_XPCOM_Components/An_Overview_of_XPCOM#CID).
Users of XPCOM can use this value to retrieve the factory using
[`Components.classesByID`](https://developer.mozilla.org/en-US/docs/Components.classesByID):

    var factory = components.classesByID[id];
    var component = factory.createInstance(Ci.nsIRequest);

This parameter is optional. If you don't supply it, the factory constructor
will generate a fresh ID. Either way, it's accessible as the value of the
factory's `id` property.

@prop [register=true] {boolean}
By default, the factory is registered as soon as it is constructed.
By including this option, set to `false`, the factory is not automatically
registered and you must register it manually using the `register()` function.

@prop [unregister=true] {boolean}
By default, the factory is unregistered as soon as the add-on which created it
is unloaded. By including this option, set to `false`, the factory is not
automatically unregistered and you must unregister it manually using
the `unregister()` function.

</api>

<api name="createInstance">
@method
Creates an instance of the component associated with this factory.

@param outer {null}
This argument must be `null`, or the function throws
`Cr.NS_ERROR_NO_AGGREGATION`.

@param iid {iid}
Interface identifier. These objects are usually accessed through
the `Components.interfaces`, or `Ci`, object. The methods of this
interface will be callable on the returned object.

If the object implements an interface that's already defined in XPCOM, you
can pass that in here:

    var about = aboutFactory.createInstance(null, Ci.nsIAboutModule);
    // You can now access the nsIAboutModule interface of the 'about' object

If you will be getting the `wrappedJSObject` property from the returned
object to access its JavaScript implementation, pass `Ci.nsISupports` here:

    var custom = factory.createInstance(null, Ci.nsISupports).wrappedJSObject;
    // You can now access the interface defined for the 'custom' object

@returns {object}
The component created by the factory.
</api>

<api name="lockFactory">
@method
This method is required by the `nsIFactory` interface, but as in most
implementations it does nothing interesting.
</api>

<api name="QueryInterface">
@method
See the documentation for
[`Unknown.QueryInterface()`](modules/sdk/platform/xpcom.html#QueryInterface(interface)).
</api>

<api name="interfaces">
@property {array}
The set of interfaces supported by this object. `Factory` sets this to
[`nsIFactory`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIFactory).
</api>

<api name="id">
@property {string}
This factory's
[class ID](https://developer.mozilla.org/en-US/docs/Creating_XPCOM_Components/An_Overview_of_XPCOM#CID).
</api>

<api name="contract">
@property {string}
This factory's
[contract ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#Contract_ID).
</api>

</api>

<api name="Service">
@class
Use this class to register an XPCOM service. To register a service for
a component, construct a `Service`, giving it:

* a constructor for the object implementing the service
* a [contract ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#Contract_ID)
and/or a
[class ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#CID)

<!-- -->

    var service = Service({
      contract: contractId,
      Component: AlertService
    });

After this, XPCOM users can access the service implementation by
supplying the contract ID:

    var alertService = Cc[contractId].getService(Ci.nsIAlertsService);
    alertService.showAlertNotification(...);

The `Service` interface is identical to the
`Factory` interface, so refer to the
[`Factory` interface documentation](modules/sdk/platform/xpcom.html#Factory)
for details.
</api>

<api name="register">
@function
Register the factory or service supplied. If the factory or service
is already registered, this function throws
[`Components.results.NS_ERROR_FACTORY_EXISTS`](https://developer.mozilla.org/en-US/docs/Table_Of_Errors).

By default, factories and services are registered automatically, so
you should only call `register()` if you have overridden the default behavior.
@param factory {object}
The factory or service to register.
</api>

<api name="unregister">
@function
Unregister the factory or service supplied. If the factory or service
is not registered, this function does nothing.

By default, factories and services are unregistered automatically when the
add-on that registered them is unloaded.

@param factory {object}
The factory or service to unregister.
</api>

<api name="isRegistered">
@function
Find out whether a factory or service is registered.

@param factory {object}

@returns {boolean}
True if the factory or service is registered, false otherwise.
</api>

<api name="autoRegister">
@function
Register a component (.manifest) file or all component files in a directory.
See [`nsIComponentRegistrar.autoRegister()`](https://developer.mozilla.org/en-US/docs/XPCOM_Interface_Reference/nsIComponentRegistrar#autoRegister%28%29)
for details.

@param path {string}
Path to a component file to be registered or a directory containing
component files to be registered.
</api>

<api name="factoryByID">
@function
Given a
[class ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#CID),
this function returns the associated factory or service. If the factory or
service isn't registered, this function returns `null`.

This function wraps [Components.ClassesByID](https://developer.mozilla.org/en-US/docs/Components.classesByID).

@param id {string}
A class ID.

@returns {object}
The factory or service identified by the class ID.
</api>

<api name="factoryByContract">
@function
Given a [contract ID](https://developer.mozilla.org/en/Creating_XPCOM_Components/An_Overview_of_XPCOM#Contract_ID)
this function returns the associated factory or service. If the factory
or service isn't registered, this function throws
[`Components.results.NS_ERROR_FACTORY_NOT_REGISTERED`](https://developer.mozilla.org/en-US/docs/Table_Of_Errors).

This function is similar to the standard `Components.classes[contractID]` with
one significant difference: that `Components.classes` is not updated
at runtime.

So if a factory is registered with the contract ID "@me.org/myComponent",
and another factory is already registered with that contract ID, then:

    Components.classes["@me.org/myComponent"]

will return the old factory, while:

    xpcom.factoryByContract("@me.org/myComponent")

will return the new one.

@param contract {string}
Contract ID of the factory or service to retrieve.

@returns {object}
The factory or service identified by the contract ID.
</api>
