Utility Process
===============

.. warning::
  As of january 2022, this process is under heavy work, and many things can
  evolve. Documentation might not always be as accurate as it should be.
  Please reach to #ipc if you intent to add a new utility.

The utility process is used to provide a simple way to implement IPC actor with
some more specific sandboxing properties, in case where you don't need or want
to deal with the extra complexity of adding a whole new process type but you
just want to apply different sandboxing policies.
To implement such an actor, you will have to follow a few steps like for
implementing the trivial example visible in `EmptyUtil
<https://phabricator.services.mozilla.com/D126402>`_:

  - Define a new IPC actor, e.g., ``PEmptyUtil`` that allows to get some string
    via ``GetSomeString()`` from the child to the parent

  - In the ``PUtilityProcess`` definition, expose a new child-level method,
    e.g., ``StartEmptyUtilService(Endpoint<PEmptyUtilChild>)``

  - Implement ``EmptyUtilChild`` and ``EmptyUtilParent`` classes both deriving
    from their ``PEmptyUtilXX``. If you want or need to run things from a
    different thread, you can have a look at ``UtilityProcessGenericActor``

  - Make sure both are refcounted

  - Expose your new service on ``UtilityProcessManager`` with a method
    performing the heavy lifting of starting your process, you can take
    inspiration from ``StartEmptyUtil()`` in the sample.

  - Ideally, this starting method should rely on `StartUtility() <https://searchfox.org/mozilla-central/rev/fb511723f821ceabeea23b123f1c50c9e93bde9d/ipc/glue/UtilityProcessManager.cpp#210-258,266>`_

  - To use ``StartUtility()`` mentioned above, please ensure that you provide
    a ``nsresult BindToUtilityProcess(RefPtr<UtilityProcessParent>
    aUtilityParent)``. Usually, it should be in charge of creating a set of
    endpoints and performing ``Bind()`` to setup the IPC. You can see some example for `Utility AudioDecoder <https://searchfox.org/mozilla-central/rev/4b3039b48c3cb67774270ebcc2a7d8624d888092/ipc/glue/UtilityAudioDecoderChild.h#31-51>`_

  - For proper user-facing exposition in ``about:processes`` you will have to also provide an actor
    name via a method ``UtilityActorName GetActorName() { return UtilityActorName::EmptyUtil; }``

    + Add member within `enum WebIDLUtilityActorName in <https://searchfox.org/mozilla-central/rev/fb511723f821ceabeea23b123f1c50c9e93bde9d/dom/chrome-webidl/ChromeUtils.webidl#686-689>`_

  - Handle reception of ``StartEmptyUtilService`` on the child side of
    ``UtilityProcess`` within ``RecvStartEmptyUtilService()``

  - In ``UtilityProcessChild::ActorDestroy``, release any resources that
    you stored a reference to in ``RecvStartEmptyUtilService()``.  This
    will probably include a reference to the ``EmptyUtilChild``.

  - The specific sandboxing requirements can be implemented by tracking
    ``SandboxingKind``, and it starts within `UtilityProcessSandboxing header
    <https://searchfox.org/mozilla-central/source/ipc/glue/UtilityProcessSandboxing.h>`_

  - Try and make sure you at least add some ``gtest`` coverage of your new
    actor, for example like in `existing gtest
    <https://searchfox.org/mozilla-central/source/ipc/glue/test/gtest/TestUtilityProcess.cpp>`_

  - Also ensure actual sandbox testing within

    + ``SandboxTest`` to start your new process,
      `<https://searchfox.org/mozilla-central/source/security/sandbox/common/test/SandboxTest.cpp>`_

    + ``SandboxTestingChildTests`` to define the test
      `<https://searchfox.org/mozilla-central/source/security/sandbox/common/test/SandboxTestingChildTests.h>`_

    + ``SandboxTestingChild`` to run your test
      `<https://searchfox.org/mozilla-central/source/security/sandbox/common/test/SandboxTestingChild.cpp>`_
