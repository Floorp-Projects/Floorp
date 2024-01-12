.. _defining_xpcom_components:

=========================
Defining XPCOM Components
=========================

This document explains how to write a :code:`components.conf` file. For
documentation on the idl format see :ref:`XPIDL`. For a tutorial on writing
a new XPCOM interface, see
:ref:`writing_xpcom_interface`.

Native XPCOM components are registered at build time, and compiled into static
data structures which allow them to be accessed with little runtime overhead.
Each module which wishes to register components must provide a manifest
describing each component it implements, its type, and how it should be
constructed.

Manifest files are Python data files registered in ``moz.build`` files in a
``XPCOM_MANIFESTS`` file list:

.. code-block:: python

    XPCOM_MANIFESTS += [
      'components.conf',
    ]

The files may define any of the following special variables:

.. code-block:: python

    # Optional: A function to be called once, the first time any component
    # listed in this manifest is instantiated.
    InitFunc = 'nsInitFooModule'
    # Optional: A function to be called at shutdown if any component listed in
    # this manifest has been instantiated.
    UnloadFunc = 'nsUnloadFooModule'

    # Optional: A processing priority, to determine how early or late the
    # manifest is processed. Defaults to 50. In practice, this mainly affects
    # the order in which unload functions are called at shutdown, with higher
    # priority numbers being called later.
    Priority = 10

    # Optional: A list of header files to include before calling init or
    # unload functions, or any legacy constructor functions.
    #
    # Any header path beginning with a `/` is loaded relative to the root of
    # the source tree, and must not rely on any local includes.
    #
    # Any relative header path must be exported.
    Headers = [
        '/foo/nsFooModule.h',
        'nsFoo.h',
    ]

    # A list of component classes provided by this module.
    Classes = [
        {
            # ...
        },
        # ...
    ]

    # A list of category registrations
    Categories = {
        'category': {
            'name': 'value',
            'other-name': ('value', ProcessSelector.MAIN_PROCESS_ONLY),
            # ...
        },
        # ...
    }

Class definitions may have the following properties:

``name`` (optional)
  If present, this component will generate an entry with the given name in the
  ``mozilla::components`` namespace in ``mozilla/Components.h``, which gives
  easy access to its CID, service, and instance constructors as (e.g.,)
  ``components::Foo::CID()``, ``components::Foo::Service()``, and
  ``components::Foo::Create()``, respectively.

``cid``
  A UUID string containing this component's CID, in the form
  ``'{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}'``.

``contract_ids`` (optional)
  A list of contract IDs to register for this class.

``categories`` (optional)
  A dict of category entries to register for this component's contract ID.
  Each key in the dict is the name of the category. Each value is either a
  string containing a single entry, or a list of entries.  Each entry is either
  a string name, or a dictionary of the form ``{'name': 'value', 'backgroundtasks':
  BackgroundTasksSelector.ALL_TASKS}``.  By default, category entries are registered
  for **no background tasks**: they have
  ``'backgroundtasks': BackgroundTasksSelector.NO_TASKS``.

``type`` (optional, default=``nsISupports``)
  The fully-qualified type of the class implementing this component. Defaults
  to ``nsISupports``, but **must** be provided if the ``init_method`` property
  is specified, or if neither the ``constructor`` nor ``legacy_constructor``
  properties are provided.

``headers`` (optional)
  A list of headers to include in order to call this component's constructor,
  in the same format as the global ``Headers`` property.

``init_method`` (optional)
  The name of a method to call on newly-created instances of this class before
  returning them. The method must take no arguments, and must return a
  ``nsresult``. If it returns failure, that failure is propagated to the
  ``getService`` or ``createInstance`` caller.

``constructor`` (optional)
  The fully-qualified name of a constructor function to call in order to
  create instances of this class. This function must be declared in one of the
  headers listed in the ``headers`` property, must take no arguments, and must
  return ``already_AddRefed<iface>`` where ``iface`` is the interface provided
  in the ``type`` property.

  This property is incompatible with ``legacy_constructor``.

``esModule`` (optional)
  If provided, must be the URL of a
  `JavaScript module <https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Modules>`_
  which contains a JavaScript implementation of the component.
  The ``constructor`` property must contain the name of an exported
  function which can be constructed to create a new instance of the component.


``jsm`` (deprecated, optional)
  Do not use. Use ``esModule`` instead.

``legacy_constructor`` (optional)
  This property is deprecated, and should not be used in new code.

  The fully-qualified name of a constructor function to call in order to
  create instances of this class. This function must be declared in one of the
  headers listed in the ``headers`` property, and must have the signature
  ``nsresult(const nsID& aIID, void** aResult)``, and behave equivalently to
  ``nsIFactory::CreateInstance``.

  This property is incompatible with ``constructor``.

``singleton`` (optional, default=``False``)
  If true, this component's constructor is expected to return the same
  singleton for every call, and no ``mozilla::components::<name>::Create()``
  method will be generated for it.

``overridable`` (optional, default=``False``)
  If true, this component's contract ID is expected to be overridden by some
  tests, and its ``mozilla::components::<name>::Service()`` getter will
  therefore look it up by contract ID for every call. This component must,
  therefore, provide at least one contract ID in its ``contract_ids`` array.

  If false, the ``Service()`` getter will always retrieve the service based on
  its static data, and it cannot be overridden.

  Note: Enabling this option is expensive, and should not be done when it can
  be avoided, or when the getter is used by any hot code.

``external`` (optional, default=``False`` if any ``headers`` are provided, ``True`` otherwise)
  If true, a constructor for this component's ``type`` must be defined in
  another translation unit, using ``NS_IMPL_COMPONENT_FACTORY(type)``. The
  constructor must return an ``already_AddRefed<nsISupports>``, and will be
  used to construct instances of this type.

  This option should only be used in cases where the headers which define the
  component's concrete type cannot be easily included without local includes.

  Note: External constructors may not specify an ``init_method``, since the
  generated code will not have the necessary type information required to call
  it. This option is also incompatible with ``constructor`` and
  ``legacy_constructor``.

``processes`` (optional, default=``ProcessSelector.ANY_PROCESS``)
  An optional specifier restricting which types of process this component may
  be loaded in. This must be a property of ``ProcessSelector`` with the same
  name as one of the values in the ``Module::ProcessSelector`` enum.


Conditional Compilation
=======================

This manifest may run any appropriate Python code to customize the values of
the ``Classes`` array based on build configuration. To simplify this process,
the following globals are available:

``defined``
  A function which returns true if the given build config setting is defined
  and true.

``buildconfig``
  The ``buildconfig`` python module, with a ``substs`` property containing a
  dict of all available build substitutions.


Component Constructors
======================

There are several ways to define component constructors, which vary mostly
depending on how old the code that uses them is:

Class Constructors
------------------

This simplest way to define a component is to include a header defining a
concrete type, and let the component manager call that class's constructor:

.. code-block:: python

  'type': 'mozilla::foo::Foo',
  'headers': ['mozilla/Foo.h'],

This is generally the preferred method of defining non-singleton constructors,
but may not be practicable for classes which rely on local includes for their
definitions.

Singleton Constructors
----------------------

Singleton classes are generally expected to provide their own constructor
function which caches a singleton instance the first time it is called, and
returns the same instance on subsequent calls. This requires declaring the
constructor in an included header, and implementing it in a separate source
file:

.. code-block:: python

  'type': 'mozilla::foo::Foo',
  'headers': ['mozilla/Foo.h'],
  'constructor': 'mozilla::Foo::GetSingleton',

``Foo.h``

.. code-block:: cpp

    class Foo final : public nsISupports {
     public:
      static already_AddRefed<Foo> GetSingleton();
    };

``Foo.cpp``

.. code-block:: cpp

    already_AddRefed<Foo> Foo::GetSingleton() {
      // ...
    }

External Constructors
---------------------

For types whose headers can't easily be included, constructors can be defined
using a template specialization on an incomplete type:

.. code-block:: python

  'type': 'mozilla::foo::Foo',
  'external: True,'

``Foo.cpp``

.. code-block:: cpp

    NS_IMPL_COMPONENT_FACTORY(Foo) {
      return do_AddRef(new Foo()).downcast<nsISupports>();
    }

Legacy Constructors
-------------------

These should not be used in new code, and are left as an exercise for the
reader.


Registering Categories
======================

Classes which need define category entries with the same value as their
contract ID may do so using the following:

.. code-block:: python

    'contract_ids': ['@mozilla.org/foo;1'],
    'categories': {
        'content-policy': 'm-foo',
        'Gecko-Content-Viewers': ['image/jpeg', 'image/png'],
    },

This will define each of the following category entries:

* ``"content-policy"`` ``"m-foo",`` ``"@mozilla.org/foo;1"``
* ``"Gecko-Content-Viewers"`` ``"image/jpeg"`` ``"@mozilla.org/foo;1"``
* ``"Gecko-Content-Viewers"`` ``"image/png"`` ``"@mozilla.org/foo;1"``

Some category entries do not have a contract ID as a value. These entries can
be specified by adding to a global ``Categories`` dictionary:

.. code-block:: python

    Categories = {
        'update-timer': {
            'nsUpdateService': '@mozilla.org/updates/update-service;1,getService,background-update-timer,app.update.interval,43200,86400',
        }
    }

It is possible to limit these on a per-process basis by using a tuple as the
value:

.. code-block:: python

    Categories = {
        '@mozilla.org/streamconv;1': {
            '?from=gzip&to=uncompressed': ('', ProcessSelector.ALLOW_IN_SOCKET_PROCESS),
        }
    }
