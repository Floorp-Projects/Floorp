Gecko Processes
===============

Before Creating a New Process
-----------------------------

Firefox started out as a one process application.  Then, one became two as NPAPI plugins like Flash were pushed into their own process (plugin processes) for security and stability reasons.  Then, it split again so that the browser could also disentangle itself from web content (content processes).  Then, implementations on some platforms developed processes for graphics ("GPU" processes).  And for media codecs.  And VR.  And file URLs.  And sockets.  And even more content processes.  And so on...

Here is an incomplete list of *good* reasons we've created new processes:

* Separating HTML and JS from the browser makes it possible to secure the browser and the rest of the system from them, even when those APIs are compromised.
* Browser stability was also improved by separating HTML and JS from the browser, since catastrophic failures related to a tab could be limited to the tab instead of crashing the browser.
* Site isolation requires additional processes to separate HTML and JS for different sites.  The separation of memory spaces undermines many types of exploits.
* Sandboxing processes offers great security guarantees but requires making tradeoffs between power and protection.  More processes means more options.  For example, we heavily sandbox content processes to protect from external code, while the File process, which is a content process that can access ``file://`` URLs, has a sandbox that is similar but allows access to local files.
* One of the benefits of the GPU process was that it improved browser stability by separating a system component that had frequent stability issues -- GPU drivers.  The same logic inspired the NPAPI (Flash) plugin process.

Informed by this history, there is some of non-obvious preparation that you should do before starting down this path.  This falls under the category of "First, do no harm":

* **Consult the Platform and IPC teams** (#ipc) to develop the plan for the way your process will integrate with the systems in which it will exist, as well as how it will be handled on any platforms where it will *not* exist.  For example, an application's process hierarchy forms a tree where one process spawns another.  Currently, all processes in Firefox are spawned by the main process (excepting the `launcher process`_).  There is good reason for this, mostly based on our sandboxing restrictions that forbid non-main processes from launching new processes themselves.  But it means that the main process will need to know to create your process.  If you make the decision to do this from, say, a content process, you will need a safe, performant and stable way to request this of the main process.  You will also need a way to efficiently communicate directly with your new process.  And you will need to consider limitations of some platforms (think Android) where you may not want to or not be able to spawn the new process.
* **Consult the sandboxing team** (#hardening) to discuss what the sandbox for your new process will look like.  Anything that compromises security is a non-starter.  You may, for instance, want to create a new process to escape the confines of the sandbox in a content process.  This can be legitimate, for example you may need access to some device API that is unavailable to a content process, but the security for your new process will then have to come from a different source.  "I won't run Javascript" is not sufficient.  Keep in mind that your process will have to have some mechanism for communication with other processes to be useful, so it is always a potential target.

.. note::
    Firefox has, to date, undergone exactly one occurrence of the *removal* of a process type.  In 2020, the NPAPI plugin process was removed when the last supported plugin, Adobe's FlashPlayer, reached its end-of-life.

.. _launcher process: https://wiki.mozilla.org/Platform/Integration/InjectEject/Launcher_Process/

Firefox Process Hierarchy
-------------------------

This diagram shows the primary process types in Firefox.  

.. mermaid::

    graph TD
        RDD -->|PRemoteDecoderManager| Content
        RDD(Data Decoder) ==>|PRDD| Main

        Launcher --> Main

        Main ==>|PContent| Content
        Main ==>|PSocketProcess| Socket(Network Socket)
        Main ==>|PGMP| GMP(Gecko Media Plugins)
        VR ==>|PVR| Main
        GPU ==>|PGPU| Main

        Socket -->|PSocketProcessBridge| Content

        GPU -->|PCompositorManager| Main
        GPU -->|PCompositorManager| Content

        Content -->|PGMPContent| GMP

        VR -->|PVRGPU| GPU

.. warning::
    The main process is sometimes called the UI process, the chrome process, the browser process or the parent process.  This is true for documentation, conversation and, most significantly, **code**.  Due to the syntactic overlap with IPDL actors, that last name can get pretty confusing.  Less commonly, the content process is called the renderer process, which is it's name in Chromium code.  Since the content process sandbox won't allow it, Firefox never does (hardware) rendering in the content/rendering process!

The arrows point from the parent side to the child.  Bolded arrows indicate the first top-level actors for the various process types.  The other arrows show important actors that are usually the first connections establised between the two processes.  These relationships difficult to discern from code.  Processes should clearly document their top-level connections in their IPDL files.

Some process types only exist on some platforms and some processes may only be created on demand.  For example, Mac builds do not use a GPU process but instead fold the same actor connections into its main process (except ``PGPU``, which it does not use).  These exceptions are also very hard to learn from code and should be clearly documented.

``about:processes`` shows statistics for the processes in a currently running browser.  It is also useful to see the distribution of web pages across content processes.

.. _Adding a New Type of Process:

Adding a New Type of Process
----------------------------

Adding a new process type doesn't require any especially difficult steps but it does require a lot of steps that are not obvious.  This section will focus on the steps as it builds an example.  It will be light on the details of the classes and protocols involved.  Some implementations may need to seek out a deeper understanding of the components set up here but most should instead strive for simplicity.

In the spirit of creating a *responsible* process, the sample will connect several components that any deployed Gecko process is likely to need.  These include configuring a sandbox, `registration with the CrashReporter service`_ and ("minimal") XPCOM initialization.  Consult documentation for these components for more information on their integration.

This example will be loosely based on the old (now defunct) IPDL **Extending a Protocol** example for adding a new actor.  We will add a command to the browser's ``navigator`` JS object, ``navigator.getAssistance()``.  When the user enters the new command in, say, the browser's console window, it will create a new process of our new "dummy" process type and ask that process for "assistance" in the form of a string that it will then print to the console.  Once that is done, the new process will be cleanly destroyed.

Code for the complete demo can be found `here <https://phabricator.services.mozilla.com/D119038>`_.

.. _registration with the CrashReporter service: `Crash Reporter`_

Common Architecture
~~~~~~~~~~~~~~~~~~~

Every type of process (besides the launcher and main processses) needs two classes and an actor pair to launch.  This sample will be adding a process type we call "dummy".

* An actor pair where the parent actor is a top-level actor in the main process and the child is the (first) top-level actor in the new process.  It is common for this actor to simply take the name of the process type.  The sample uses ``PDummy``, so it creates ``DummyParent`` and ``DummyChild`` actor subclasses as usual (see :ref:`IPDL: Inter-Thread and Inter-Process Message Passing`).
* A subclass of `GeckoChildProcessHost <https://searchfox.org/mozilla-central/source/ipc/glue/GeckoChildProcessHost.h>`_ that exists in the main process (where new processes are created) and handles most of the machinery needed for new process creation.  It is common for these names to be the process type plus ``ProcessParent`` or ``ProcessHost``.  The sample uses ``DummyParent::Host``, a private class, which keeps ``GeckoChildProcessHost`` out of the Dummy process' *public interface* since it is large, complicated and mostly unimportant externally.  This complexity is also why it is a bad idea to add extra responsibilities to the ``Host`` object that inherits it.
* A subclass of `ProcessChild <https://searchfox.org/mozilla-central/source/ipc/glue/ProcessChild.h>`_ that exists in the new process.  These names are usually generated by affixing ``ProcessChild`` or ``ProcessImpl`` to the type.  The sample will use ``DummyChild::Process``, another private class, for the same reasons it did with the ``Host``.

A fifth class is optional but integration with common services requires something like it:

* A singleton class that "manages" the collective of processes (usually the Host objects) of the new type in the main process.  In many instances, there is at most one instance of a process type, so this becomes a singleton that manages a singleton... that manages a singleton.  Object ownership is often hard to establish between manager objects and the hosts they manage.  It is wise to limit the power of these classes.  This class will often get its name by appending ``ProcessManager`` to the process type.  The sample provides a very simple manager in ``DummyParent::Manager``.

Finally, it is highly probable and usually desirable for the new process to include another new top-level actor that represents the top-level operations and communications of the new process.  This actor will use the new process as a child but may have any other process as the parent, unlike ``PDummy`` whose parent is always the main process.  This new actor will be created by the main process, which creates a pair of ``Endpoint`` objects specifically for the desired process pairing, and then sends those ``Endpoint`` objects to their respective processes.  The Dummy example is interesting because the user can issue the command from a content process or the main one, by opening the console in a normal or a privileged page (e.g. ``about:sessionrestore``), respectively.  Supporting both of these cases will involve very little additional effort.  The sample will show this as part of implementing the second top-level actor pair ``PDummyHotline`` in `Connecting With Other Processes`_, where the parent can be in either the main or a content process.

The rest of the sections will explain how to compose these classes and integrate them with Gecko.

Process Bookkeeping
~~~~~~~~~~~~~~~~~~~

To begin with, look at the `geckoprocesstypes generator <https://searchfox.org/mozilla-central/source/xpcom/geckoprocesstypes_generator/geckoprocesstypes/__init__.py>`_ which adds the bones for a new process (by defining enum values and so on).  It contains instructions on additional steps that are required to add the new process type.

Creating the New Process
~~~~~~~~~~~~~~~~~~~~~~~~

The sample does this in ``DummyParent::LaunchDummyProcess``.  The core behavior is fairly clear:

.. code-block:: c++

    /* static */
    bool DummyParent::LaunchDummyProcess(
            base::ProcessId aParentPid, LaunchDummyProcessResolver&& aResolver) {
        UniqueHost host(new Host(aParentPid, std::move(aResolver)));

        // Prepare "command line" startup args for new process
        std::vector<std::string> extraArgs;
        if (!host->BuildProcessArgs(&extraArgs)) {
          return false;
        }

        // Async launch creates a promise that we use below.
        if (!host->AsyncLaunch(extraArgs)) {
          return false;
        }

        host->WhenProcessHandleReady()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [host = std::move(host)](
              const ipc::ProcessHandlePromise::ResolveOrRejectValue&
                  aResult) mutable {
            if (aResult.IsReject()) {
              host->ResolveAsFailure();
              return;
            }

            new DummyParent(std::move(host));
          });
    }

First, it creates an object of our ``GeckoChildProcessHost`` subclass (storing some stuff for later).  ``GeckoChildProcessHost`` is a base class that abstracts the system-level operations involved in launching the new process.  It is the most substantive part of the launch procedure.  After its construction, the code prepares a bunch of strings to pass on the "command line", which is the only way to pass data to the new process before IPDL is established.  All new processes will at least include ``-parentBuildId`` for validating that dynamic libraries are properly versioned, and shared memory for passing user preferences, which can affect early process behavior.  Finally, it tells ``GeckoChildProcessHost`` to asynchronously launch the process and run the given lambda when it has a result.  The lambda creates ``DummyParent`` with the new host, if successful.

In this sample, the ``DummyParent`` is owned (in the reference-counting sense) by IPDL, which is why it doesn't get assigned to anything.  This simplifies the design dramatically.  IPDL takes ownership when the actor calls ``Open`` in its constructor:

.. code-block:: c++

    DummyParent::DummyParent(UniqueHost&& aHost)
        : mHost(std::move(aHost)) {
      Open(mHost->TakeInitialPort(),
           base::GetProcId(mHost->GetChildProcessHandle()));
      // ...
      mHost->MakeBridgeAndResolve();
    }

After the ``Open`` call, the actor is live and communication with the new process can begin.  The constructor concludes by initiating the process of connecting the ``PDummyHotline`` actors; ``Host::MakeBridgeAndResolve`` will be covered in `
Creating a New Top Level Actor`_.  However, before we get into that, we should finish defining the lifecycle of the process.  In the next section we look at launching the new process from the new process' perspective.

.. warning::
    The code could have chosen to create a ``DummyChild`` instead of a ``DummyParent`` and the choice may seem cosmetic but there are substantial implications to the choice that could affect browser stability.  The most significant is that the prohibitibition on synchronous IPDL messages going from parent to child can no longer guarantee freedom from multiprocess deadlock.

Initializing the New Process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The new process first adopts the Dummy process type in ``XRE_InitChildProcess``, where it responds to the Dummy values we added to some enums above.  Specifically, we need to choose the type of MessageLoop our main thread will run (this is discussed later) and we need to create our ``ProcessChild`` subclass.  This is not an insignificant choice so pay close attention to the `MessageLoop` options:

.. code-block:: c++

    MessageLoop::Type uiLoopType;
    switch (XRE_GetProcessType()) {
      case GeckoProcessType_Dummy:
        uiLoopType = MessageLoop::TYPE_MOZILLA_CHILD;  break;
      // ...
    }

    // ...

    UniquePtr<ProcessChild> process;
    switch (XRE_GetProcessType()) {
        // ...
        case GeckoProcessType_Dummy:
          process = MakeUnique<DummyChild::Process>(parentPID);
          break;
    }

We then need to create our singleton ``DummyChild`` object, which can occur in the constructor or the ``Process::Init()`` call, which is common.  We store a strong reference to the actor (as does IPDL) so that we are guaranteed that it exists as long as the ``ProcessChild`` does -- although the message channel may be closed.  We will release the reference either when the process is properly shutting down or when an IPC error closes the channel.

``Init`` is given the command line arguments constucted above so it will need to be overridden to parse them.  In the same, it does this, binds our actor by calling ``Open`` as was done with the parent, then initializes a bunch of components that the process expects to use.

.. code-block:: c++

    bool DummyChild::Init(int aArgc, char* aArgv[]) {
    #if defined(MOZ_SANDBOX) && defined(OS_WIN)
      mozilla::SandboxTarget::Instance()->StartSandbox();
    #elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
      StartOpenBSDSandbox(GeckoProcessType_Dummy);
    #endif

      // ... command line parsing ...

      ipc::SharedPreferenceDeserializer deserializer;
      if (!deserializer.DeserializeFromSharedMemory(prefsHandle, prefMapHandle,
                                                    prefsLen, prefMapSize)) {
        return false;
      }

      if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
        return false;
      }

      if (NS_WARN_IF(!Open(ipc::IOThreadChild::TakeInitialPort(), mParentPid))) {
        return false;
      }

      // ... initializing components ...

      if (NS_FAILED(NS_InitMinimalXPCOM())) {
        return false;
      }

      return true;
    }

This is a slimmed down version of the real ``Init`` method.  We see that it establishes a sandbox (more on this later) and then reads the command line and preferences that we sent from the main process.  It then initializes the thread manager, which is required by for the subsequent ``Open`` call.

Among the list of components we initialize in the sample code, XPCOM is special.  XPCOM includes a suite of components, including the component manager, and is usually required for serious Gecko development.  It is also heavyweight and should be avoided if possible.  We will leave the details of XPCOM development to that module but we mention XPCOM configuration that is special to new processes, namely ``ProcessSelector``.    ``ProcessSelector`` is used to determine what process types have access to what XPCOM components.  By default, a process has access to none.  The code adds ``ALLOW_IN_GPU_RDD_VR_SOCKET_AND_DUMMY_PROCESS`` to the ``ProcessSelector`` enum in `gen_static_components.py <https://searchfox.org/mozilla-central/source/xpcom/components/gen_static_components.py>`_ and `Module.h <https://searchfox.org/mozilla-central/source/xpcom/components/Module.h>`_.  It then updates the selectors in various ``components.conf`` files and hardcoded spots like ``nsComponentManager.cpp`` to add the Dummy processes to the list that can use them.  Some modules are required to bootstrap XPCOM and will cause it to fail to initialize if they are not permitted.

At this point, the new process is idle, waiting for messages from the main process that will start the ``PDummyHotline`` actor.  We discuss that in `Creating a New Top Level Actor`_ below but, first, let's look at how the main and dummy processes will handle clean destruction.

Destroying the New Process
~~~~~~~~~~~~~~~~~~~~~~~~~~

Gecko processes have a clean way for clients to request that they shutdown.  Simply calling ``Close()`` on the top level actor at either endoint will begin the shutdown procedure (so, ``PDummyParent::Close`` or ``PDummyChild::Close``).  The only other way for a child process to terminate is to crash.  Each of these three options requires some special handling.

.. note::
    There is no need to consider the case where the parent (main) process crashed, because the Dummy process would be quickly terminated by Gecko.

In cases where ``Close()`` is called, the shutdown procedure is fairly straightforward.  Once the call completes, the actor is no longer connected to a channel -- messages will not be sent or received, as is the case with a normal top-level actor (or any managed actor after calling ``Send__delete__()``).  In the sample code, we ``Close`` the ``DummyChild`` when some (as yet unwritten) dummy process code calls ``DummyChild::Shutdown``.  

.. code-block:: c++

    /* static */
    void DummyChild::Shutdown() {
      if (gDummyChild) {
        // Wait for the other end to get everything we sent before shutting down.
        // We never want to Close during a message (response) handler, so
        // we dispatch a new runnable.
        auto dp = gDummyChild;
        RefPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
            "DummyChild::FinishShutdown",
            [dp2 = std::move(gDummyChild)]() { dp2->Close(); });
        dp->SendEmptyMessageQueue(
            [runnable](bool) { NS_DispatchToMainThread(runnable); },
            [runnable](mozilla::ipc::ResponseRejectReason) {
              NS_DispatchToMainThread(runnable);
            });
      }
    }

The comment in the code makes two important points:

* ``Close`` should never be called from a message handler (e.g. in a ``RecvFoo`` method).  We schedule it to run later.
* If the ``DummyParent`` hasn't finished handling messages the ``DummyChild`` sent, or vice-versa, those messages will be lost.  For that reason, we have a trivial sentinel message ``EmptyMessageQueue`` that we simply send and wait to respond before we ``Close``.  This guarantees that the main process will have handled all of the messages we sent before it.  Because we know the details of the ``PDummy`` protocol, we know that this means we won't lose any important messages this way.  Note that we say "important" messages because we could still lose messages sent *from* the main process.  For example, a ``RequestMemoryReport`` message sent by the MemoryReporter could be lost.  The actor would need a more complex shutdown protocol to catch all of these messages but in our case there would be no point.  A process that is terminating is probably not going to produce useful memory consumption data.  Those messages can safely be lost.

`Debugging Process Startup`_ looks at what happens if we omit the ``EmptyMessageQueue`` message.

We can also see that, once the ``EmptyMessageQueue`` response is run, we are releasing ``gDummyChild``, which will result in the termination of the process.

.. code-block:: c++

    DummyChild::~DummyChild() {
      // ...
      XRE_ShutdownChildProcess();
    }

At this point, the ``DummyParent`` in the main process is alerted to the channel closure because IPDL will call its :ref:`ActorDestroy <Actor Lifetimes in C++>` method.

.. code-block:: c++

    void DummyParent::ActorDestroy(ActorDestroyReason aWhy) {
      if (aWhy == AbnormalShutdown) {
        GenerateCrashReport(OtherPid());
      }
      // ...
    }

IPDL then releases its (sole) reference to ``DummyParent`` and the destruction of the process apparatus is complete.

The ``ActorDestroy`` code shows how we handle the one remaining shutdown case: a crash in the Dummy process.  In this case, IPDL will *detect* the dead process and free the ``DummyParent`` actor as above, only with an ``AbnormalShutdown`` reason.  We generate a crash report, which requires crash reporter integration, but no additional "special" steps need to be taken.

Creating a New Top Level Actor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We now have a framework that creates the new process and connects it to the main process.  We now want to make another top-level actor but this one will be responsible for our intended behavior (not just bootstrapping the new process).  Above, we saw that this is started by ``Host::MakeBridgeAndResolve`` after the ``DummyParent`` connection is established.

.. code-block:: c++

    bool DummyParent::Host::MakeBridgeAndResolve() {
      ipc::Endpoint<PDummyHotlineParent> parent;
      ipc::Endpoint<PDummyHotlineChild> child;

      auto resolveFail = MakeScopeExit([&] { mResolver(Nothing()); });

      // Parent side is first PID (main/content), child is second (dummy).
      nsresult rv = PDummyHotline::CreateEndpoints(
          mParentPid, base::GetProcId(GetChildProcessHandle()), &parent, &child);

      // ...

      if (!mActor->SendCreateDummyHotlineChild(std::move(child))) {
        NS_WARNING("Failed to SendCreateDummyHotlineChild");
        return false;
      }

      resolveFail.release();
      mResolver(Some(std::move(parent)));
      return true;
    }

Because the operation of launching a process is asynchronous, we have configured this so that it creates the two endpoints for the new top-level actors, then we send the child one to the new process and resolve a promise with the other.  The dummy process creates its ``PDummyHotlineChild`` easily:

.. code-block:: c++

    mozilla::ipc::IPCResult DummyChild::RecvCreateDummyHotlineChild(
        Endpoint<PDummyHotlineChild>&& aEndpoint) {
      mDummyHotlineChild = new DummyHotlineChild();
      if (!aEndpoint.Bind(mDummyHotlineChild)) {
        return IPC_FAIL(this, "Unable to bind DummyHotlineChild");
      }
      return IPC_OK();
    }

``MakeProcessAndGetAssistance`` binds the same way:

.. code-block:: c++

    RefPtr<DummyHotlineParent> dummyHotlineParent = new DummyHotlineParent();
    if (!endpoint.Bind(dummyHotlineParent)) {
      NS_WARNING("Unable to bind DummyHotlineParent");
      return false;
    }
    MOZ_ASSERT(ok);

However, the parent may be in the main process or in content.  We handle both cases in the next section.

.. _Connecting With Other Processes:

Connecting With Other Processes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``DummyHotlineParent::MakeProcessAndGetAssistance`` is the method that we run from either the main or the content process and that should kick off the procedure that will result in sending a string (that we get from a new Dummy process) to a DOM promise.  It starts by constructing a different promise -- one like the ``mResolver`` in ``Host::MakeBridgeAndResolve`` in the last section that produced a ``Maybe<Endpoint<PDummyHotlineParent>>``.  In the main process, we just make the promise ourselves and call ``DummyParent::LaunchDummyProcess`` to start the procedure that will result in it being resolved as already described.  If we are calling from the content process, we simply write an async ``PContent`` message that calls ``DummyParent::LaunchDummyProcess`` and use the message handler's promise as our promise.

.. code-block:: c++

    /* static */
    bool DummyHotlineParent::MakeProcessAndGetAssistance(
        RefPtr<mozilla::dom::Promise> aPromise) {
      RefPtr<LaunchDummyProcessPromise> resolver;

      if (XRE_IsContentProcess()) {
        auto* contentChild = mozilla::dom::ContentChild::GetSingleton();
        MOZ_ASSERT(contentChild);

        resolver = contentChild->SendLaunchDummyProcess();
      } else {
        MOZ_ASSERT(XRE_IsParentProcess());
        auto promise = MakeRefPtr<LaunchDummyProcessPromise::Private>(__func__);
        resolver = promise;

        if (!DummyParent::LaunchDummyProcess(
                base::GetCurrentProcId(),
                [promise = std::move(promise)](
                    Maybe<Endpoint<PDummyHotlineParent>>&& aMaybeEndpoint) mutable {
                  promise->Resolve(std::move(aMaybeEndpoint), __func__);
                })) {
          NS_WARNING("Failed to launch Dummy process");
          resolver->Reject(NS_ERROR_FAILURE);
          return false;
        }
      }

      resolver->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aPromise](Maybe<Endpoint<PDummyHotlineParent>>&& maybeEndpoint) mutable {
            if (!maybeEndpoint) {
              aPromise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            RefPtr<DummyHotlineParent> dummyHotlineParent = new DummyHotlineParent();
            Endpoint<PDummyHotlineParent> endpoint = maybeEndpoint.extract();
            if (!endpoint.Bind(dummyHotlineParent)) {
              NS_WARNING("Unable to bind DummyHotlineParent");
              return false;
            }
            MOZ_ASSERT(ok);

            // ... communicate with PDummyHotline and write message to console ...
          },
          [aPromise](mozilla::ipc::ResponseRejectReason&& aReason) {
            aPromise->MaybeReject(NS_ERROR_FAILURE);
          });

      return true;
    }

    mozilla::ipc::IPCResult ContentParent::RecvLaunchDummyProcess(
        LaunchDummyProcessResolver&& aResolver) {
      if (!DummyParent::LaunchDummyProcess(OtherPid(),
                                           std::move(aResolver))) {
        NS_WARNING("Failed to launch Dummy process");
      }
      return IPC_OK();
    }

To summarize, connecting processes always requires endpoints to be constructed by the main process, even when neither process being connected is the main process.  It is the only process that creates ``Endpoint`` objects.  From that point, connecting is just a matter of sending the endpoints to the right processes, constructing an actor for them, and then calling ``Endpoint::Bind``.

Completing the Sample
~~~~~~~~~~~~~~~~~~~~~

We have covered the main parts needed for the sample.  Now we just need to wire it all up.  First, we add the command to ``Navigator.webidl`` and ``Navigator.h``/``Navigator.cpp``:

.. code-block:: c++

    partial interface Navigator {
      [Throws]
      Promise<DOMString> getAssistance();
    };

    already_AddRefed<Promise> Navigator::GetAssistance(ErrorResult& aRv) {
      if (!mWindow || !mWindow->GetDocShell()) {
        aRv.Throw(NS_ERROR_UNEXPECTED);
        return nullptr;
      }

      RefPtr<Promise> echoPromise = Promise::Create(mWindow->AsGlobal(), aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      if (!DummyHotlineParent::MakeProcessAndGetAssistance(echoPromise)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }

      return echoPromise.forget();
    }

Then, we need to add the part that gets the string we use to resolve the promise in ``MakeProcessAndGetAssistance`` (or reject it if it hasn't been resolved by the time ``ActorDestroy`` is called):

.. code-block:: c++

    /* static */
    bool DummyHotlineParent::MakeProcessAndGetAssistance(
        RefPtr<mozilla::dom::Promise> aPromise) {

        // ... construct and connect dummyHotlineParent ...

        RefPtr<DummyPromise> promise = dummyHotlineParent->mPromise.Ensure(__func__);
        promise->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [dummyHotlineParent, aPromise](nsString aMessage) mutable {
              aPromise->MaybeResolve(aMessage);
            },
            [dummyHotlineParent, aPromise](nsresult aErr) mutable {
              aPromise->MaybeReject(aErr);
            });

        if (!dummyHotlineParent->SendRequestAssistance()) {
          NS_WARNING("DummyHotlineParent::SendRequestAssistance failed");
        }
    }

    mozilla::ipc::IPCResult DummyHotlineParent::RecvAssistance(
        nsString&& aMessage, const AssistanceResolver& aResolver) {
      mPromise.Resolve(aMessage, __func__);
      aResolver(true);
      return IPC_OK();
    }

    void DummyHotlineParent::ActorDestroy(ActorDestroyReason aWhy) {
      mPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    }

The ``DummyHotlineChild`` has to respond to the ``RequestAssistance`` method, which it does by returning a string and then calling ``Close`` on itself when the string has been received (but we do not call ``Close`` in the ``Recv`` method!).  We use an async response to the ``GiveAssistance`` message to detect that the string was received.  During closing, the actor's ``ActorDestroy`` method then calls the ``DummyChild::Shutdown`` method we defined in `Destroying the New Process`_:

.. code-block:: c++

    mozilla::ipc::IPCResult DummyHotlineChild::RecvRequestAssistance() {
      RefPtr<DummyHotlineChild> me = this;
      RefPtr<nsIRunnable> runnable =
          NS_NewRunnableFunction("DummyHotlineChild::Close", [me]() { me->Close(); });

      SendAssistance(
          nsString(HelpMessage()),
          [runnable](bool) { NS_DispatchToMainThread(runnable); },
          [runnable](mozilla::ipc::ResponseRejectReason) {
            NS_DispatchToMainThread(runnable);
          });

      return IPC_OK();
    }

    void DummyHotlineChild::ActorDestroy(ActorDestroyReason aWhy) {
      DummyChild::Shutdown();
    }

During the Dummy process lifetime, there are two references to the ``DummyHotlineChild``, one from IPDL and one from the ``DummyChild``.  The call to ``Close`` releases the one held by IPDL and the other isn't released until the ``DummyChild`` is destroyed.

Running the Sample
~~~~~~~~~~~~~~~~~~

To run the sample, build and run and open the console.  The new command is ``navigator.getAssistance().then(console.log)``.  The message sent by ``SendAssistance`` is then logged to the console.  The sample code also includes the name of the type of process that was used for the ``DummyHotlineParent`` so you can confirm that it works from main and from content.

Debugging Process Startup
-------------------------

Debugging a child process at the start of its life is tricky.  With most platforms/toolchains, it is surprisingly difficult to connect a debugger before the main routine begins execution.  You may also find that console logging is not yet established by the operating system, especially when working with sandboxed child processes.  Gecko has some facilities that make this less painful.

.. _Debugging with IPDL Logging:

Debugging with IPDL Logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is also best seen with an example.  To start, we can create a bug in the sample by removing the ``EmptyMessageQueue`` message sent to ``DummyParent``.  This message was intended to guarantee that the ``DummyParent`` had handled all messages sent before it, so we could ``Close`` with the knowledge that we didn't miss anything.  This sort of bug can be very difficult to track down because it is likely to be intermittent and may manifest more easily on some platforms/architectures than others.  To create this bug, replace the ``SendEmptyMessageQueue`` call in ``DummyChild::Shutdown``:

.. code-block:: c++

    auto dc = gDummyChild;
    RefPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
        "DummyChild::FinishShutdown",
        [dc2 = std::move(gDummyChild)]() { dc2->Close(); });
    dc->SendEmptyMessageQueue(
        [runnable](bool) { NS_DispatchToMainThread(runnable); },
        [runnable](mozilla::ipc::ResponseRejectReason) {
          NS_DispatchToMainThread(runnable);
        });

with just an (asynchronous) call to ``Close``:

.. code-block:: c++

    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "DummyChild::FinishShutdown",
        [dc = std::move(gDummyChild)]() { dc->Close(); }));

When we run the sample now, everything seems to behave ok but we see messages like these in the console: ::

    ###!!! [Parent][RunMessage] Error: (msgtype=0x410001,name=PDummy::Msg_InitCrashReporter) Channel closing: too late to send/recv, messages will be lost

    [Parent 16672, IPC I/O Parent] WARNING: file c:/mozilla-src/mozilla-unified/ipc/chromium/src/base/process_util_win.cc:167
    [Parent 16672, Main Thread] WARNING: Not resolving response because actor is dead.: file c:/mozilla-src/mozilla-unified/ipc/glue/ProtocolUtils.cpp:931
    [Parent 16672, Main Thread] WARNING: IPDL resolver dropped without being called!: file c:/mozilla-src/mozilla-unified/ipc/glue/ProtocolUtils.cpp:959

We could probably figure out what is happening here from the messages but, with more complex protocols, understanding what led to this may not be so easy.  To begin diagnosing, we can turn on IPC Logging, which was defined in the IPDL section on :ref:`Message Logging`.  We just need to set an environment variable before starting the browser.  Let's turn it on for all ``PDummy`` and ``PDummyHotline`` actors: ::

    MOZ_IPC_MESSAGE_LOG="PDummyParent,PDummyChild,PDummyHotlineParent,PDummyHotlineChild"

To underscore what we said above, when logging is active, the change in timing makes the error message go away and everything closes properly on a tested Windows desktop.  However, the issue remains on a Macbook Pro and the log shows the issue rather clearly: ::

    [time: 1627075553937959][63096->63085] [PDummyChild] Sending  PDummy::Msg_InitCrashReporter
    [time: 1627075553949441][63085->63096] [PDummyParent] Sending  PDummy::Msg_CreateDummyHotlineChild
    [time: 1627075553950293][63092->63096] [PDummyHotlineParent] Sending  PDummyHotline::Msg_RequestAssistance
    [time: 1627075553979151][63096<-63085] [PDummyChild] Received  PDummy::Msg_CreateDummyHotlineChild
    [time: 1627075553979433][63096<-63092] [PDummyHotlineChild] Received  PDummyHotline::Msg_RequestAssistance
    [time: 1627075553979498][63096->63092] [PDummyHotlineChild] Sending  PDummyHotline::Msg_GiveAssistance
    [time: 1627075553980105][63092<-63096] [PDummyHotlineParent] Received  PDummyHotline::Msg_GiveAssistance
    [time: 1627075553980181][63092->63096] [PDummyHotlineParent] Sending reply  PDummyHotline::Reply_GiveAssistance
    [time: 1627075553980449][63096<-63092] [PDummyHotlineChild] Received  PDummyHotline::Reply_GiveAssistance
    [tab 63092] NOTE: parent actor received `Goodbye' message.  Closing channel.
    [default 63085] NOTE: parent actor received `Goodbye' message.  Closing channel.
    [...]
    ###!!! [Parent][RunMessage] Error: (msgtype=0x420001,name=PDummy::Msg_InitCrashReporter) Channel closing: too late to send/recv, messages will be lost
    [...]
    [default 63085] NOTE: parent actor received `Goodbye' message.  Closing channel.

The imbalance with ``Msg_InitCrashReporter`` is clear.  The message was not *Received* before the channel was closed.  Note that the first ``Goodbye`` for the main (default) process is for the ``PDummyHotline`` actor -- in this case, its child actor was in a content (tab) process.  The second default process ``Goodbye`` is from the dummy process, sent when doing ``Close``.  It might seem that it should handle the ``Msg_InitCrashReporter`` if it can handle the later ``Goodbye`` but this does not happen for safety reasons.

Early Debugging For A New Process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let's assume now that we still don't understand the problem -- maybe we don't know that the ``InitCrashReporter`` message is sent internally by the ``CrashReporterClient`` we initialized.  Or maybe we're only looking at Windows builds.  We decide we'd like to be able to hook a debugger to the new process so that we can break on the ``SendInitCrashReporter`` call.  Attaching the debugger has to happen fast -- process startup probably completes in under a second.  Debugging this is not always easy.

Windows users have options that work with both the Visual Studio and WinDbg debuggers.  For Visual Studio users, there is an easy-to-use VS addon called the `Child Process Debugging Tool`_ that allows you to connect to *all* processes that are launched by a process you are debugging.  So, if the VS debugger is connected to the main process, it will automatically connect to the new dummy process (and every other launched process) at the point that they are spawned.  This way, the new process never does anything outside of the debugger.  Breakpoints, etc work as expected.  The addon mostly works like a toggle and will remain on until it is disabled from the VS menu.

WinDbg users can achieve essentially the same behavior with the `.childdbg`_ command.  See the docs for details but essentially all there is to know is that ``.childdbg 1`` enables it and ``.childdbg 0`` disables it.  You might add it to a startup config file (see the WinDbg ``-c`` command line option)

Linux and mac users should reference gdb's ``detach-on-fork``.  The command to debug child processes is ``set detach-on-fork off``.  Again, the behavior is largely what you would expect -- that all spawned processes are added to the current debug session.  The command can be added to ``.gdbinit`` for ease.  At the time of this writing, lldb does not support automatically connecting to newly spawned processes.

Finally, Linux users can use ``rr`` for time-travel debugging.  See `Debugging Firefox with rr`_ for details.

These solutions are not always desirable.  For example, the fact that they hook *all* spawned processes can mean that targeting breakpoints to one process requires us to manually disconnect many other processes.  In these cases, an easier solution may be to use Gecko environment variables that will cause the process to sleep for some number of seconds.  During that time, you can find the process ID (PID) for the process you want to debug and connect your debugger to it.  OS tools like ``ProcessMonitor`` can give you the PID but it will also be clearly logged to the console just before the process waits.

Set ``MOZ_DEBUG_CHILD_PROCESS=1`` to turn on process startup pausing.  You can also set ``MOZ_DEBUG_CHILD_PAUSE=N`` where N is the number of seconds to sleep.  The default is 10 seconds on Windows and 30 on other platforms.

Pausing for the debugger is not a panacea.  Since the environmental varaiables are not specific to process type, you will need to wait for all of the processes Gecko creates while you wait for it to get to yours.  The pauses can also end up exposing unknown concurrency bugs in the browser before it even gets to your issue, which is good to discover but doesn't fix your bug.  That said, any of these strategies would be enough to facilitate easily breaking on ``SendInitCrashReporter`` and finding our sender.

.. _Child Process Debugging Tool: https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool
.. _.childdbg: https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/-childdbg--debug-child-processes-


