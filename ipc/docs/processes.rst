Gecko Processes
===============

Before Creating a New Process
-----------------------------

Firefox started out as a one process application.  Then, one became two as
NPAPI plugins like Flash were pushed into their own process (plugin processes)
for security and stability reasons.  Then, it split again so that the browser
could also disentangle itself from web content (content processes).  Then,
implementations on some platforms developed processes for graphics ("GPU"
processes).  And for media codecs.  And VR.  And file URLs.  And sockets.  And
even more content processes.  And so on...

Here is an incomplete list of *good* reasons we've created new processes:

* Separating HTML and JS from the browser makes it possible to secure the
  browser and the rest of the system from them, even when those APIs are
  compromised.
* Browser stability was also improved by separating HTML and JS from the
  browser, since catastrophic failures related to a tab could be limited to the
  tab instead of crashing the browser.
* Site isolation requires additional processes to separate HTML and JS for
  different sites.  The separation of memory spaces undermines many types of
  exploits.
* Sandboxing processes offers great security guarantees but requires making
  tradeoffs between power and protection.  More processes means more options.
  For example, we heavily sandbox content processes to protect from external
  code, while the File process, which is a content process that can access
  ``file://`` URLs, has a sandbox that is similar but allows access to local
  files.
* One of the benefits of the GPU process was that it improved browser
  stability by separating a system component that had frequent stability
  issues -- GPU drivers.  The same logic inspired the NPAPI (Flash) plugin
  process.

Informed by this history, there is some of non-obvious preparation that you
should do before starting down this path.  This falls under the category of
"First, do no harm":

* **Consult the Platform and IPC teams** (#ipc) to develop the plan for the
  way your process will integrate with the systems in which it will exist, as
  well as how it will be handled on any platforms where it will *not* exist.
  For example, an application's process hierarchy forms a tree where one process
  spawns another.  Currently, all processes in Firefox are spawned by the main
  process (excepting the `launcher process`_).  There is good reason for this,
  mostly based on our sandboxing restrictions that forbid non-main processes
  from launching new processes themselves.  But it means that the main process
  will need to know to create your process.  If you make the decision to do
  this from, say, a content process, you will need a safe, performant and
  stable way to request this of the main process.  You will also need a way to
  efficiently communicate directly with your new process.  And you will need to
  consider limitations of some platforms (think Android) where you may not want
  to or not be able to spawn the new process.
* **Consult the sandboxing team** (#hardening) to discuss what the sandbox for
  your new process will look like.  Anything that compromises security is a
  non-starter.  You may, for instance, want to create a new process to escape
  the confines of the sandbox in a content process.  This can be legitimate,
  for example you may need access to some device API that is unavailable to a
  content process, but the security for your new process will then have to come
  from a different source.  "I won't run Javascript" is not sufficient.  Keep
  in mind that your process will have to have some mechanism for communication
  with other processes to be useful, so it is always a potential target.

.. note::
    Firefox has, to date, undergone exactly one occurrence of the *removal* of
    a process type.  In 2020, the NPAPI plugin process was removed when the
    last supported plugin, Adobe's FlashPlayer, reached its end-of-life.

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
    The main process is sometimes called the UI process, the chrome process,
    the browser process or the parent process.  This is true for documentation,
    conversation and, most significantly, **code**.  Due to the syntactic
    overlap with IPDL actors, that last name can get pretty confusing.  Less
    commonly, the content process is called the renderer process, which is it's
    name in Chromium code.  Since the content process sandbox won't allow it,
    Firefox never does (hardware) rendering in the content/rendering process!

The arrows point from the parent side to the child.  Bolded arrows indicate the
first top-level actors for the various process types.  The other arrows show
important actors that are usually the first connections establised between the
two processes.  These relationships difficult to discern from code.  Processes
should clearly document their top-level connections in their IPDL files.

Some process types only exist on some platforms and some processes may only be
created on demand.  For example, Mac builds do not use a GPU process but
instead fold the same actor connections into its main process (except ``PGPU``,
which it does not use).  These exceptions are also very hard to learn from code
and should be clearly documented.

``about:processes`` shows statistics for the processes in a currently running
browser.  It is also useful to see the distribution of web pages across content
processes.

.. _Adding a New Type of Process:

Adding a New Type of Process
----------------------------

Adding a new process type doesn't require any especially difficult steps but it
does require a lot of steps that are not obvious.  This section will focus on
the steps as it builds an example.  It will be light on the details of the
classes and protocols involved.  Some implementations may need to seek out a
deeper understanding of the components set up here but most should instead
strive for simplicity.

In the spirit of creating a *responsible* process, the sample will connect
several components that any deployed Gecko process is likely to need.  These
include configuring a sandbox, `registration with the CrashReporter service`_
and ("minimal") XPCOM initialization.  Consult documentation for these
components for more information on their integration.

This example will be loosely based on the old (now defunct) IPDL **Extending a
Protocol** example for adding a new actor.  We will add a command to the
browser's ``navigator`` JS object, ``navigator.getAssistance()``.  When the
user enters the new command in, say, the browser's console window, it will
create a new process of our new **Demo** process type and ask that process for
"assistance" in the form of a string that it will then print to the console.
Once that is done, the new process will be cleanly destroyed.

Code for the complete demo can be found `here
<https://phabricator.services.mozilla.com/D119038>`_.

.. _registration with the CrashReporter service: `Crash Reporter`_

Common Architecture
~~~~~~~~~~~~~~~~~~~

Every type of process (besides the launcher and main processses) needs two
classes and an actor pair to launch.  This sample will be adding a process type
we call **Demo**.

* An actor pair where the parent actor is a top-level actor in the main process
  and the child is the (first) top-level actor in the new process.  It is common
  for this actor to simply take the name of the process type.  The sample uses
  ``PDemo``, so it creates ``DemoParent`` and ``DemoChild`` actor subclasses
  as usual (see :ref:`IPDL: Inter-Thread and Inter-Process Message Passing`).
* A subclass of `GeckoChildProcessHost
  <https://searchfox.org/mozilla-central/source/ipc/glue/GeckoChildProcessHost.h>`_
  that exists in the main process (where new processes are created) and handles
  most of the machinery needed for new process creation.  It is common for these
  names to be the process type plus ``ProcessParent`` or ``ProcessHost``.  The
  sample uses ``DemoParent::Host``, a private class, which keeps
  ``GeckoChildProcessHost`` out of the **Demo** process' *public interface*
  since it is large, complicated and mostly unimportant externally.  This
  complexity is also why it is a bad idea to add extra responsibilities to the
  ``Host`` object that inherits it.
* A subclass of `ProcessChild
  <https://searchfox.org/mozilla-central/source/ipc/glue/ProcessChild.h>`_ that
  exists in the new process.  These names are usually generated by affixing
  ``ProcessChild`` or ``ProcessImpl`` to the type.  The sample will use
  ``DemoChild::Process``, another private class, for the same reasons it did
  with the ``Host``.

A fifth class is optional but integration with common services requires
something like it:

* A singleton class that "manages" the collective of processes (usually the
  Host objects) of the new type in the main process.  In many instances, there
  is at most one instance of a process type, so this becomes a singleton that
  manages a singleton... that manages a singleton.  Object ownership is often
  hard to establish between manager objects and the hosts they manage.  It is
  wise to limit the power of these classes.  This class will often get its name
  by appending ``ProcessManager`` to the process type.  The sample provides a
  very simple manager in ``DemoParent::Manager``.

Finally, it is highly probable and usually desirable for the new process to
include another new top-level actor that represents the top-level operations
and communications of the new process.  This actor will use the new process as
a child but may have any other process as the parent, unlike ``PDemo`` whose
parent is always the main process.  This new actor will be created by the main
process, which creates a pair of ``Endpoint`` objects specifically for the
desired process pairing, and then sends those ``Endpoint`` objects to their
respective processes.  The **Demo** example is interesting because the user can
issue the command from a content process or the main one, by opening the
console in a normal or a privileged page (e.g. ``about:sessionrestore``),
respectively.  Supporting both of these cases will involve very little
additional effort.  The sample will show this as part of implementing the
second top-level actor pair ``PDemoHelpline`` in `Connecting With Other
Processes`_, where the parent can be in either the main or a content process.

The rest of the sections will explain how to compose these classes and
integrate them with Gecko.

Process Bookkeeping
~~~~~~~~~~~~~~~~~~~

To begin with, look at the `geckoprocesstypes generator
<https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/geckoprocesstypes_generator/geckoprocesstypes/__init__.py>`_
which adds the bones for a new process (by defining enum values and so on).
Some further manual intervention is still required, and you need to follow the
following checklists depending on your needs.

Basic requirements
^^^^^^^^^^^^^^^^^^

* Add a new entry to the `enum WebIDLProcType
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/dom/chrome-webidl/ChromeUtils.webidl#610-638>`_
* Update the `static_assert
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/xre/nsAppRunner.cpp#988-990>`_
  call checking for boundary against ``GeckoProcessType_End``
* Add your process to the correct ``MessageLoop::TYPE_x`` in the first
  ``switch(XRE_GetProcessType())`` in `XRE_InitChildProcess
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/xre/nsEmbedFunctions.cpp#572-590>`_.
  You can get more information about that topic in `this comment
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/ipc/chromium/src/base/message_loop.h#159-187>`_
* Instantiate your child within the second ``switch (XRE_GetProcessType())`` in
  `XRE_InitChildProcess
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/xre/nsEmbedFunctions.cpp#615-671>`_
* Add a new entry ``PROCESS_TYPE_x`` in `nsIXULRuntime interface
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/system/nsIXULRuntime.idl#183-196>`_

Graphics
########

If you need graphics-related interaction, hack into `gfxPlatform
<https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/gfx/thebes/gfxPlatform.cpp>`_

- Add a call to your process manager init in ``gfxPlatform::Init()`` in
  `gfxPlatform
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/gfx/thebes/gfxPlatform.cpp#808-810>`_
- Add a call to your process manager shutdown in ``gfxPlatform::Shutdown()`` in
  `gfxPlatform
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/gfx/thebes/gfxPlatform.cpp#1255-1259>`_

Android
#######

You might want to talk with `#geckoview` maintainers to ensure if this is
required or applicable to your new process type.

- Add a new ``<service>`` entry against
  ``org.mozilla.gecko.process.GeckoChildProcessServices$XXX`` in the
  `AndroidManifest
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/mobile/android/geckoview/src/main/AndroidManifest.xml#45-81>`_
- Add matching class inheritance from `GeckoChildProcessServices
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/mobile/android/geckoview/src/main/java/org/mozilla/gecko/process/GeckoChildProcessServices.jinja#10-13>`_
- Add new entry in `public enum GeckoProcessType
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/mobile/android/geckoview/src/main/java/org/mozilla/gecko/process/GeckoProcessType.java#11-23>`_

Crash reporting
###############

- Add ``InitCrashReporter`` message to the parent-side `InitCrashReporter
  <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/PUtilityProcess.ipdl#30>`_
- Ensure your parent class inherits `public ipc::CrashReporterHelper<GeckoProcessType_Xxx>
  <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessParent.h#23>`_
- Add new ``Xxx*Status`` `annotations
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/crashreporter/CrashAnnotations.yaml#968-971>`_
  entry for your new process type description. The link here points to
  `UtilityProcessStatus` so you can see the similar description you have to
  write, but you might want to respect ordering in that file and put your new
  code at the appropriate place.
- Add entry in `PROCESS_CRASH_SUBMIT_ATTEMPT
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/components/telemetry/Histograms.json#13403-13422>`_

Memory reporting
################

Throughout the linked code, please consider those methods more as boilerplate code that will require some trivial modification to fit your exact usecase.

- Add definition of memory reporter to your new :ref:`top-level actor <Top Level Actors>`

  + Type inclusion `MemoryReportTypes <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/PUtilityProcess.ipdl#6>`_
  + To parent-side `AddMemoryReport <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/PUtilityProcess.ipdl#32>`_
  + To child-side `RequestMemoryReport <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/PUtilityProcess.ipdl#44-48>`_

- Add handling for your new process within `nsMemoryReporterManager::GetReportsExtended <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/xpcom/base/nsMemoryReporterManager.cpp#1813-1819>`_
- Provide a process manager level abstraction

  + Implement a new class deriving ``MemoryReportingProcess`` such as `UtilityMemoryReporter <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessManager.cpp#253-292>`_
  + Write a `GetProcessMemoryReport <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessManager.cpp#294-300>`_

- On the child side, provide an implementation for `RequestMemoryReport <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessChild.cpp#153-166>`_
- On the parent side

  + Provide an implementation for `RequestMemoryReport <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessParent.cpp#41-69>`_
  + Provide an implementation for `AddMemoryReport <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessParent.cpp#71-77>`_

If you want to add a test that ensures proper behavior, you can have a look at the `utility process memory report test <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/test/browser/browser_utility_memoryReport.js>`_

Process reporting
#################

Those elements will be used for exposing processes to users in some `about:`
pages. You might want to ping `#fluent-reviewers` to ensure if you need your
process there.

- Add a `user-facing localizable name
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/locales/en-US/toolkit/global/processTypes.ftl#39-57>`_
  for your process, if needed
- Hashmap from process type to user-facing string above in `const ProcessType
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/modules/ProcessType.jsm#14-20>`_
- For `about:processes` you will probably want to follow the following steps:

  + Add handling for your new process type producing a unique `fluentName <https://searchfox.org/mozilla-central/rev/be4604e4be8c71b3c1dbff2398a5b05f15411673/toolkit/components/aboutprocesses/content/aboutProcesses.js#472-539>`_, i.e., constructing a dynamic name is highly discouraged
  + Add matching localization strings within `fluent localization file <https://searchfox.org/mozilla-central/rev/be4604e4be8c71b3c1dbff2398a5b05f15411673/toolkit/locales/en-US/toolkit/about/aboutProcesses.ftl#35-55>`_

Profiler
########

- Add definition of ``PProfiler`` to your new IPDL

  + Type inclusion `protocol PProfiler <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/PUtilityProcess.ipdl#9>`_
  + Child-side `InitProfiler <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/PUtilityProcess.ipdl#42>`_

- Make sure your initialization path contains a `SendInitProfiler <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessHost.cpp#222-223>`_. You will want to perform the call once a ``OnChannelConnected`` is issued, thus ensuring your new process is connected to IPC.
- Provide an implementation for `InitProfiler <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessChild.cpp#147-151>`_

- You will probably want to make sure your child process code register within the profiler a proper name, otherwise it will default to ``GeckoMain`` ; this can be done by issuing ``profiler_set_process_name(nsCString("XxX"))`` on the child init side.

Static Components
#################

The amount of changes required here are significant, `Bug 1740485: Improve
StaticComponents code generation
<https://bugzilla.mozilla.org/show_bug.cgi?id=1740485>`_ tracks improving that.

- Update allowance in those configuration files to match new process selector
  that includes your new process. When exploring those components definitions,
  keep in mind that you are looking at updating `processes` field in the
  `Classes` object. The `ProcessSelector` value will come from what the reader
  writes based on the instructions below. Some of these also contains several
  services, so you might have to ensure you have all your bases covered. Some of
  the components might not need to be updated as well.

  + `libpref <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/modules/libpref/components.conf>`_
  + `telemetry <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/components/telemetry/core/components.conf>`_
  + `android <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/widget/android/components.conf>`_
  + `gtk <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/widget/gtk/components.conf>`_
  + `windows <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/widget/windows/components.conf>`_
  + `base <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/base/components.conf>`_
  + `components <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/components/components.conf>`_
  + `ds <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/ds/components.conf>`_
  + `threads <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/threads/components.conf>`_
  + `cocoa kWidgetModule <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/widget/cocoa/nsWidgetFactory.mm#194-202>`_
  + `build <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/build/components.conf>`_
  + `XPCOMinit kXPCOMModule <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/build/XPCOMInit.cpp#172-180>`_

- Within `static components generator
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/components/gen_static_components.py>`_

  + Add new definition in ``ProcessSelector`` for your new process
    ``ALLOW_IN_x_PROCESS = 0x..``
  + Add new process selector masks including your new process definition
  + Also add those into the ``PROCESSES`` structure

- Within `module definition <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/components/Module.h>`_

  + Add new definition in ``enum ProcessSelector``
  + Add new process selector mask including the new definition
  + Update ``kMaxProcessSelector``

- Within `nsComponentManager <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/xpcom/components/nsComponentManager.cpp>`_

  + Add new selector match in ``ProcessSelectorMatches`` for your new process
    (needed?)
  + Add new process selector for ``gProcessMatchTable`` in
    ``nsComponentManagerImpl::Init()``

Glean telemetry
###############

- Ensure your new IPDL includes on the child side

  + `FlushFOGData
    <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/PUtilityProcess.ipdl#55>`_
  + `TestTriggerMetrics
    <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/PUtilityProcess.ipdl#60>`_

- Provide a parent-side implementation for `FOGData
  <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessParent.cpp#79-82>`_
- Provide a child-side implementation for `FlushFOGData
  <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessChild.cpp#179-183>`_
- Child-side should flush its FOG data at IPC `ActorDestroy
  <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessChild.cpp#199-201>`_
- Child-side `test metrics
  <https://searchfox.org/mozilla-central/rev/fc4d4a8d01b0e50d20c238acbb1739ccab317ebc/ipc/glue/UtilityProcessChild.cpp#185-191>`_
- Within `FOGIPC
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/components/glean/ipc/FOGIPC.cpp>`_

  + Add handling of your new process type within ``FlushAllChildData()`` `here
    <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/components/glean/ipc/FOGIPC.cpp#106-121>`_
    and ``SendFOGData()`` `here
    <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/components/glean/ipc/FOGIPC.cpp#165-182>`_
  + Add support for sending test metrics in ``TestTriggerMetrics()`` `here
    <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/components/glean/ipc/FOGIPC.cpp#208-232>`_

- Handle process shutdown in ``register_process_shutdown()`` of `glean
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/toolkit/components/glean/api/src/ipc.rs>`_

Sandboxing
##########

Sandboxing changes related to a new process can be non-trivial, so it is
strongly advised that you reach to the Sandboxing team in ``#hardening`` to
discuss your needs prior to making changes.

Linux Sandbox
_____________

Linux sandboxing mostly works by allowing / blocking system calls for child
process and redirecting (brokering) some from the child to the parent. Rules
are written in a specific DSL: `BPF
<https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/chromium/sandbox/linux/bpf_dsl/bpf_dsl.h#21-72>`_.

- Add new ``SetXXXSandbox()`` function within `linux sandbox
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/Sandbox.cpp#719-748>`_
- Within `sandbox filter
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/SandboxFilter.cpp>`_

  + Add new helper ``GetXXXSandboxPolicy()`` `like this one
    <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/SandboxFilter.cpp#2036-2040>`_
    called by ``SetXXXSandbox()``
  + Derive new class `similar to this
    <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/SandboxFilter.cpp#2000-2034>`_
    inheriting ``SandboxPolicyCommon`` or ``SandboxPolicyBase`` and defining
    the sandboxing policy

- Add new ``SandboxBrokerPolicyFactory::GetXXXProcessPolicy()`` in `sandbox
  broker
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/broker/SandboxBrokerPolicyFactory.cpp#881-932>`_
- Add new case handling in ``GetEffectiveSandboxLevel()`` in `sandbox launch
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/launch/SandboxLaunch.cpp#243-271>`_
- Add new entry in ``enum class ProcType`` of `sandbox reporter header
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/reporter/SandboxReporterCommon.h#32-39>`_
- Add new case handling in ``SubmitToTelemetry()`` in `sandbox reporter
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/reporter/SandboxReporter.cpp#131-152>`_
- Add new case handling in ``SandboxReportWrapper::GetProcType()`` of `sandbox
  reporter wrapper
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/linux/reporter/SandboxReporterWrappers.cpp#69-91>`_

MacOS Sandbox
_____________

- Add new case handling in ``GeckoChildProcessHost::StartMacSandbox()`` of
  `GeckoChildProcessHost <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/ipc/glue/GeckoChildProcessHost.cpp#1720-1743>`_
- Add new entry in ``enum MacSandboxType`` defined in `macOS sandbox header
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/mac/Sandbox.h#12-20>`_
- Within `macOS sandbox core
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/mac/Sandbox.mm>`_
  handle the new ``MacSandboxType`` in

   + ``MacSandboxInfo::AppendAsParams()`` in the `switch statement
     <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/mac/Sandbox.mm#164-188>`_
   + ``StartMacSandbox()`` in the `serie of if/else statements
     <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/mac/Sandbox.mm#286-436>`_.
     This code sets template values for the sandbox string rendering, and is
     running on the side of the main process.
   + ``StartMacSandboxIfEnabled()`` in this `switch statement
     <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/mac/Sandbox.mm#753-782>`_.
     You might also need a ``GetXXXSandboxParamsFromArgs()`` that performs CLI
     parsing on behalf of ``StartMacSandbox()``.

- Create the new sandbox definition file
  ``security/sandbox/mac/SandboxPolicy<XXX>.h`` for your new process ``<XXX>``,
  and make it exposed in the ``EXPORTS.mozilla`` section of `moz.build
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/mac/moz.build#7-13>`_.
  Those rules follows a specific Scheme-like language. You can learn more about
  it in `Apple Sandbox Guide
  <https://reverse.put.as/wp-content/uploads/2011/09/Apple-Sandbox-Guide-v1.0.pdf>`_
  as well as on your system within ``/System/Library/Sandbox/Profiles/``.

Windows Sandbox
_______________

- Introduce a new ``SandboxBroker::SetSecurityLevelForXXXProcess()`` that
  defines the new sandbox in both

  + the sandbox broker basing yourself on that `example
    <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/win/src/sandboxbroker/sandboxBroker.cpp#1241-1344>`_
  + the remote sandbox broker getting `inspired by
    <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/win/src/remotesandboxbroker/remoteSandboxBroker.cpp#161-165>`_

- Add new case handling in ``WindowsProcessLauncher::DoSetup()`` calling
  ``SandboxBroker::SetSecurityLevelForXXXProcess()`` in `GeckoChildProcessHost
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/ipc/glue/GeckoChildProcessHost.cpp#1391-1470>`_.
  This will apply actual sandboxing rules to your process.

Sandbox tests
_____________

- New process' first top level actor needs to `include PSandboxTesting
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/common/test/PSandboxTesting.ipdl>`_
  and implement ``RecvInitSandboxTesting`` `like there
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/ipc/glue/UtilityProcessChild.cpp#165-174>`_.
- Add your new process ``string_name`` in the ``processTypes`` list of `sandbox
  tests <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/test/browser_sandbox_test.js#17>`_
- Add a new case in ``SandboxTest::StartTests()`` in `test core
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/common/test/SandboxTest.cpp#100-232>`_
  to handle your new process
- Add a new if branch for your new process in ``SandboxTestingChild::Bind()``
  in `testing child
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/common/test/SandboxTestingChild.cpp#68-96>`_
- Add a new ``RunTestsXXX`` function for your new process (called by ``Bind()``
  above) `similar to that implementation
  <https://searchfox.org/mozilla-central/rev/d4b9c457db637fde655592d9e2048939b7ab2854/security/sandbox/common/test/SandboxTestingChildTests.h#333-363>`_

Creating the New Process
~~~~~~~~~~~~~~~~~~~~~~~~

The sample does this in ``DemoParent::LaunchDemoProcess``.  The core
behavior is fairly clear:

.. code-block:: c++

    /* static */
    bool DemoParent::LaunchDemoProcess(
            base::ProcessId aParentPid, LaunchDemoProcessResolver&& aResolver) {
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

            auto actor = MakeRefPtr<DemoParent>(std::move(host));
            actor->Init();
          });
    }

First, it creates an object of our ``GeckoChildProcessHost`` subclass (storing
some stuff for later).  ``GeckoChildProcessHost`` is a base class that
abstracts the system-level operations involved in launching the new process.
It is the most substantive part of the launch procedure.  After its
construction, the code prepares a bunch of strings to pass on the "command
line", which is the only way to pass data to the new process before IPDL is
established.  All new processes will at least include ``-parentBuildId`` for
validating that dynamic libraries are properly versioned, and shared memory for
passing user preferences, which can affect early process behavior.  Finally, it
tells ``GeckoChildProcessHost`` to asynchronously launch the process and run
the given lambda when it has a result.  The lambda creates ``DemoParent`` with
the new host, if successful.

In this sample, the ``DemoParent`` is owned (in the reference-counting sense)
by IPDL, which is why it doesn't get assigned to anything.  This simplifies the
design dramatically.  IPDL takes ownership when the actor calls ``Bind`` from
the ``Init`` method:

.. code-block:: c++

    DemoParent::DemoParent(UniqueHost&& aHost)
        : mHost(std::move(aHost)) {}

    DemoParent::Init() {
      mHost->TakeInitialEndpoint().Bind(this);
      // ...
      mHost->MakeBridgeAndResolve();
    }

After the ``Bind`` call, the actor is live and communication with the new
process can begin.  The constructor concludes by initiating the process of
connecting the ``PDemoHelpline`` actors; ``Host::MakeBridgeAndResolve`` will be
covered in `Creating a New Top Level Actor`_.  However, before we get into
that, we should finish defining the lifecycle of the process.  In the next
section we look at launching the new process from the new process' perspective.

.. warning::
    The code could have chosen to create a ``DemoChild`` instead of a
    ``DemoParent`` and the choice may seem cosmetic but it has substantial
    implications that could affect browser stability.  The most
    significant is that the prohibitibition on synchronous IPDL messages going
    from parent to child can no longer guarantee freedom from multiprocess
    deadlock.

Initializing the New Process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The new process first adopts the **Demo** process type in
``XRE_InitChildProcess``, where it responds to the **Demo** values we added to
some enums above.  Specifically, we need to choose the type of MessageLoop our
main thread will run (this is discussed later) and we need to create our
``ProcessChild`` subclass.  This is not an insignificant choice so pay close
attention to the `MessageLoop` options:

.. code-block:: c++

    MessageLoop::Type uiLoopType;
    switch (XRE_GetProcessType()) {
      case GeckoProcessType_Demo:
        uiLoopType = MessageLoop::TYPE_MOZILLA_CHILD;  break;
      // ...
    }

    // ...

    UniquePtr<ProcessChild> process;
    switch (XRE_GetProcessType()) {
        // ...
        case GeckoProcessType_Demo:
          process = MakeUnique<DemoChild::Process>(parentPID);
          break;
    }

We then need to create our singleton ``DemoChild`` object, which can occur in
the constructor or the ``Process::Init()`` call, which is common.  We store a
strong reference to the actor (as does IPDL) so that we are guaranteed that it
exists as long as the ``ProcessChild`` does -- although the message channel may
be closed.  We will release the reference either when the process is properly
shutting down or when an IPC error closes the channel.

``Init`` is given the command line arguments constucted above so it will need
to be overridden to parse them.  It does this, binds our actor by
calling ``Bind`` as was done with the parent, then initializes a bunch of
components that the process expects to use:

.. code-block:: c++

    bool DemoChild::Init(int aArgc, char* aArgv[]) {
    #if defined(MOZ_SANDBOX) && defined(OS_WIN)
      mozilla::SandboxTarget::Instance()->StartSandbox();
    #elif defined(__OpenBSD__) && defined(MOZ_SANDBOX)
      StartOpenBSDSandbox(GeckoProcessType_Demo);
    #endif

      if (!mozilla::ipc::ProcessChild::InitPrefs(aArgc, aArgv)) {
        return false;
      }

      if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
        return false;
      }

      if (NS_WARN_IF(!TakeInitialEndpoint().Bind(this))) {
        return false;
      }

      // ... initializing components ...

      if (NS_FAILED(NS_InitMinimalXPCOM())) {
        return false;
      }

      return true;
    }

This is a slimmed down version of the real ``Init`` method.  We see that it
establishes a sandbox (more on this later) and then reads the command line and
preferences that we sent from the main process.  It then initializes the thread
manager, which is required by for the subsequent ``Bind`` call.

Among the list of components we initialize in the sample code, XPCOM is
special.  XPCOM includes a suite of components, including the component
manager, and is usually required for serious Gecko development.  It is also
heavyweight and should be avoided if possible.  We will leave the details of
XPCOM development to that module but we mention XPCOM configuration that is
special to new processes, namely ``ProcessSelector``.    ``ProcessSelector``
is used to determine what process types have access to what XPCOM components.
By default, a process has access to none.  The code adds enums for selecting
a subset of process types, like
``ALLOW_IN_GPU_RDD_VR_SOCKET_UTILITY_AND_DEMO_PROCESS``, to the
``ProcessSelector`` enum in `gen_static_components.py
<https://searchfox.org/mozilla-central/source/xpcom/components/gen_static_components.py>`_
and `Module.h
<https://searchfox.org/mozilla-central/source/xpcom/components/Module.h>`_.
It then updates the selectors in various ``components.conf`` files and
hardcoded spots like ``nsComponentManager.cpp`` to add the **Demo** processes
to the list that can use them.  Some modules are required to bootstrap XPCOM
and will cause it to fail to initialize if they are not permitted.

At this point, the new process is idle, waiting for messages from the main
process that will start the ``PDemoHelpline`` actor.  We discuss that in
`Creating a New Top Level Actor`_ below but, first, let's look at how the main
and **Demo** processes will handle clean destruction.

Destroying the New Process
~~~~~~~~~~~~~~~~~~~~~~~~~~

Gecko processes have a clean way for clients to request that they shutdown.
Simply calling ``Close()`` on the top level actor at either endoint will begin
the shutdown procedure (so, ``PDemoParent::Close`` or ``PDemoChild::Close``).
The only other way for a child process to terminate is to crash.  Each of these
three options requires some special handling.

.. note::
    There is no need to consider the case where the parent (main) process
    crashed, because the **Demo** process would be quickly terminated by Gecko.

In cases where ``Close()`` is called, the shutdown procedure is fairly
straightforward.  Once the call completes, the actor is no longer connected to
a channel -- messages will not be sent or received, as is the case with any
normal top-level actor (or any managed actor after calling
``Send__delete__()``).  In the sample code, we ``Close`` the ``DemoChild``
when some (as yet unwritten) **Demo** process code calls
``DemoChild::Shutdown``.

.. code-block:: c++

    /* static */
    void DemoChild::Shutdown() {
      if (gDemoChild) {
        // Wait for the other end to get everything we sent before shutting down.
        // We never want to Close during a message (response) handler, so
        // we dispatch a new runnable.
        auto dc = gDemoChild;
        RefPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
            "DemoChild::FinishShutdown",
            [dc2 = std::move(gDemoChild)]() { dc2->Close(); });
        dc->SendEmptyMessageQueue(
            [runnable](bool) { NS_DispatchToMainThread(runnable); },
            [runnable](mozilla::ipc::ResponseRejectReason) {
              NS_DispatchToMainThread(runnable);
            });
      }
    }

The comment in the code makes two important points:

* ``Close`` should never be called from a message handler (e.g. in a
  ``RecvFoo`` method).  We schedule it to run later.
* If the ``DemoParent`` hasn't finished handling messages the ``DemoChild``
  sent, or vice-versa, those messages will be lost.  For that reason, we have a
  trivial sentinel message ``EmptyMessageQueue`` that we simply send and wait
  to respond before we ``Close``.  This guarantees that the main process will
  have handled all of the messages we sent before it.  Because we know the
  details of the ``PDemo`` protocol, we know that this means we won't lose any
  important messages this way.  Note that we say "important" messages because
  we could still lose messages sent *from* the main process.  For example, a
  ``RequestMemoryReport`` message sent by the MemoryReporter could be lost.
  The actor would need a more complex shutdown protocol to catch all of these
  messages but in our case there would be no point.  A process that is
  terminating is probably not going to produce useful memory consumption data.
  Those messages can safely be lost.

`Debugging Process Startup`_ looks at what happens if we omit the
``EmptyMessageQueue`` message.

We can also see that, once the ``EmptyMessageQueue`` response is run, we are
releasing ``gDemoChild``, which will result in the termination of the process.

.. code-block:: c++

    DemoChild::~DemoChild() {
      // ...
      XRE_ShutdownChildProcess();
    }

At this point, the ``DemoParent`` in the main process is alerted to the
channel closure because IPDL will call its :ref:`ActorDestroy <Actor Lifetimes
in C++>` method.

.. code-block:: c++

    void DemoParent::ActorDestroy(ActorDestroyReason aWhy) {
      if (aWhy == AbnormalShutdown) {
        GenerateCrashReport(OtherPid());
      }
      // ...
    }

IPDL then releases its (sole) reference to ``DemoParent`` and the destruction
of the process apparatus is complete.

The ``ActorDestroy`` code shows how we handle the one remaining shutdown case:
a crash in the **Demo** process.  In this case, IPDL will *detect* the dead
process and free the ``DemoParent`` actor as above, only with an
``AbnormalShutdown`` reason.  We generate a crash report, which requires crash
reporter integration, but no additional "special" steps need to be taken.

Creating a New Top Level Actor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

We now have a framework that creates the new process and connects it to the
main process.  We now want to make another top-level actor but this one will be
responsible for our intended behavior, not just bootstrapping the new process.
Above, we saw that this is started by ``Host::MakeBridgeAndResolve`` after the
``DemoParent`` connection is established.

.. code-block:: c++

    bool DemoParent::Host::MakeBridgeAndResolve() {
      ipc::Endpoint<PDemoHelplineParent> parent;
      ipc::Endpoint<PDemoHelplineChild> child;

      auto resolveFail = MakeScopeExit([&] { mResolver(Nothing()); });

      // Parent side is first PID (main/content), child is second (demo).
      nsresult rv = PDempHelpline::CreateEndpoints(
          mParentPid, base::GetProcId(GetChildProcessHandle()), &parent, &child);

      // ...

      if (!mActor->SendCreateDemoHelplineChild(std::move(child))) {
        NS_WARNING("Failed to SendCreateDemoHelplineChild");
        return false;
      }

      resolveFail.release();
      mResolver(Some(std::move(parent)));
      return true;
    }

Because the operation of launching a process is asynchronous, we have
configured this so that it creates the two endpoints for the new top-level
actors, then we send the child one to the new process and resolve a promise
with the other.  The **Demo** process creates its ``PDemoHelplineChild``
easily:

.. code-block:: c++

    mozilla::ipc::IPCResult DemoChild::RecvCreateDemoHelplineChild(
        Endpoint<PDemoHelplineChild>&& aEndpoint) {
      mDemoHelplineChild = new DemoHelplineChild();
      if (!aEndpoint.Bind(mDemoHelplineChild)) {
        return IPC_FAIL(this, "Unable to bind DemoHelplineChild");
      }
      return IPC_OK();
    }

``MakeProcessAndGetAssistance`` binds the same way:

.. code-block:: c++

    RefPtr<DemoHelplineParent> demoHelplineParent = new DemoHelplineParent();
    if (!endpoint.Bind(demoHelplineParent)) {
      NS_WARNING("Unable to bind DemoHelplineParent");
      return false;
    }
    MOZ_ASSERT(ok);

However, the parent may be in the main process or in content.  We handle both
cases in the next section.

.. _Connecting With Other Processes:

Connecting With Other Processes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``DemoHelplineParent::MakeProcessAndGetAssistance`` is the method that we run
from either the main or the content process and that should kick off the
procedure that will result in sending a string (that we get from a new **Demo**
process) to a DOM promise.  It starts by constructing a different promise --
one like the ``mResolver`` in ``Host::MakeBridgeAndResolve`` in the last
section that produced a ``Maybe<Endpoint<PDemoHelplineParent>>``.  In the main
process, we just make the promise ourselves and call
``DemoParent::LaunchDemoProcess`` to start the procedure that will result in
it being resolved as already described.  If we are calling from the content
process, we simply write an async ``PContent`` message that calls
``DemoParent::LaunchDemoProcess`` and use the message handler's promise as
our promise:

.. code-block:: c++

    /* static */
    bool DemoHelplineParent::MakeProcessAndGetAssistance(
        RefPtr<mozilla::dom::Promise> aPromise) {
      RefPtr<LaunchDemoProcessPromise> resolver;

      if (XRE_IsContentProcess()) {
        auto* contentChild = mozilla::dom::ContentChild::GetSingleton();
        MOZ_ASSERT(contentChild);

        resolver = contentChild->SendLaunchDemoProcess();
      } else {
        MOZ_ASSERT(XRE_IsParentProcess());
        auto promise = MakeRefPtr<LaunchDemoProcessPromise::Private>(__func__);
        resolver = promise;

        if (!DemoParent::LaunchDemoProcess(
                base::GetCurrentProcId(),
                [promise = std::move(promise)](
                    Maybe<Endpoint<PDemoHelplineParent>>&& aMaybeEndpoint) mutable {
                  promise->Resolve(std::move(aMaybeEndpoint), __func__);
                })) {
          NS_WARNING("Failed to launch Demo process");
          resolver->Reject(NS_ERROR_FAILURE);
          return false;
        }
      }

      resolver->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [aPromise](Maybe<Endpoint<PDemoHelplineParent>>&& maybeEndpoint) mutable {
            if (!maybeEndpoint) {
              aPromise->MaybeReject(NS_ERROR_FAILURE);
              return;
            }

            RefPtr<DemoHelplineParent> demoHelplineParent = new DemoHelplineParent();
            Endpoint<PDemoHelplineParent> endpoint = maybeEndpoint.extract();
            if (!endpoint.Bind(demoHelplineParent)) {
              NS_WARNING("Unable to bind DemoHelplineParent");
              return false;
            }
            MOZ_ASSERT(ok);

            // ... communicate with PDemoHelpline and write message to console ...
          },
          [aPromise](mozilla::ipc::ResponseRejectReason&& aReason) {
            aPromise->MaybeReject(NS_ERROR_FAILURE);
          });

      return true;
    }

    mozilla::ipc::IPCResult ContentParent::RecvLaunchDemoProcess(
        LaunchDemoProcessResolver&& aResolver) {
      if (!DemoParent::LaunchDemoProcess(OtherPid(),
                                          std::move(aResolver))) {
        NS_WARNING("Failed to launch Demo process");
      }
      return IPC_OK();
    }

To summarize, connecting processes always requires endpoints to be constructed
by the main process, even when neither process being connected is the main
process.  It is the only process that creates ``Endpoint`` objects.  From that
point, connecting is just a matter of sending the endpoints to the right
processes, constructing an actor for them, and then calling ``Endpoint::Bind``.

Completing the Sample
~~~~~~~~~~~~~~~~~~~~~

We have covered the main parts needed for the sample.  Now we just need to wire
it all up.  First, we add the new JS command to ``Navigator.webidl`` and
``Navigator.h``/``Navigator.cpp``:

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

      if (!DemoHelplineParent::MakeProcessAndGetAssistance(echoPromise)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }

      return echoPromise.forget();
    }

Then, we need to add the part that gets the string we use to resolve the
promise in ``MakeProcessAndGetAssistance`` (or reject it if it hasn't been
resolved by the time ``ActorDestroy`` is called):

.. code-block:: c++

    using DemoPromise = MozPromise<nsString, nsresult, true>;

    /* static */
    bool DemoHelplineParent::MakeProcessAndGetAssistance(
        RefPtr<mozilla::dom::Promise> aPromise) {

        // ... construct and connect demoHelplineParent ...

        RefPtr<DemoPromise> promise = demoHelplineParent->mPromise.Ensure(__func__);
        promise->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [demoHelplineParent, aPromise](nsString aMessage) mutable {
              aPromise->MaybeResolve(aMessage);
            },
            [demoHelplineParent, aPromise](nsresult aErr) mutable {
              aPromise->MaybeReject(aErr);
            });

        if (!demoHelplineParent->SendRequestAssistance()) {
          NS_WARNING("DemoHelplineParent::SendRequestAssistance failed");
        }
    }

    mozilla::ipc::IPCResult DemoHelplineParent::RecvAssistance(
        nsString&& aMessage, const AssistanceResolver& aResolver) {
      mPromise.Resolve(aMessage, __func__);
      aResolver(true);
      return IPC_OK();
    }

    void DemoHelplineParent::ActorDestroy(ActorDestroyReason aWhy) {
      mPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    }

The ``DemoHelplineChild`` has to respond to the ``RequestAssistance`` method,
which it does by returning a string and then calling ``Close`` on itself when
the string has been received (but we do not call ``Close`` in the ``Recv``
method!).  We use an async response to the ``GiveAssistance`` message to detect
that the string was received.  During closing, the actor's ``ActorDestroy``
method then calls the ``DemoChild::Shutdown`` method we defined in `Destroying
the New Process`_:

.. code-block:: c++

    mozilla::ipc::IPCResult DemoHelplineChild::RecvRequestAssistance() {
      RefPtr<DemoHelplineChild> me = this;
      RefPtr<nsIRunnable> runnable =
          NS_NewRunnableFunction("DemoHelplineChild::Close", [me]() { me->Close(); });

      SendAssistance(
          nsString(HelpMessage()),
          [runnable](bool) { NS_DispatchToMainThread(runnable); },
          [runnable](mozilla::ipc::ResponseRejectReason) {
            NS_DispatchToMainThread(runnable);
          });

      return IPC_OK();
    }

    void DemoHelplineChild::ActorDestroy(ActorDestroyReason aWhy) {
      DemoChild::Shutdown();
    }

During the **Demo** process lifetime, there are two references to the
``DemoHelplineChild``, one from IPDL and one from the ``DemoChild``.  The call
to ``Close`` releases the one held by IPDL and the other isn't released until
the ``DemoChild`` is destroyed.

Running the Sample
~~~~~~~~~~~~~~~~~~

To run the sample, build and run and open the console.  The new command is
``navigator.getAssistance().then(console.log)``.  The message sent by
``SendAssistance`` is then logged to the console.  The sample code also
includes the name of the type of process that was used for the
``DemoHelplineParent`` so you can confirm that it works from main and from
content.

Debugging Process Startup
-------------------------

Debugging a child process at the start of its life is tricky.  With most
platforms/toolchains, it is surprisingly difficult to connect a debugger before
the main routine begins execution.  You may also find that console logging is
not yet established by the operating system, especially when working with
sandboxed child processes.  Gecko has some facilities that make this less
painful.

.. _Debugging with IPDL Logging:

Debugging with IPDL Logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~

This is also best seen with an example.  To start, we can create a bug in the
sample by removing the ``EmptyMessageQueue`` message sent to ``DemoParent``.
This message was intended to guarantee that the ``DemoParent`` had handled all
messages sent before it, so we could ``Close`` with the knowledge that we
didn't miss anything.  This sort of bug can be very difficult to track down
because it is likely to be intermittent and may manifest more easily on some
platforms/architectures than others.  To create this bug, replace the
``SendEmptyMessageQueue`` call in ``DemoChild::Shutdown``:

.. code-block:: c++

    auto dc = gDemoChild;
    RefPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
        "DemoChild::FinishShutdown",
        [dc2 = std::move(gDemoChild)]() { dc2->Close(); });
    dc->SendEmptyMessageQueue(
        [runnable](bool) { NS_DispatchToMainThread(runnable); },
        [runnable](mozilla::ipc::ResponseRejectReason) {
          NS_DispatchToMainThread(runnable);
        });

with just an (asynchronous) call to ``Close``:

.. code-block:: c++

    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "DemoChild::FinishShutdown",
        [dc = std::move(gDemoChild)]() { dc->Close(); }));

When we run the sample now, everything seems to behave ok but we see messages
like these in the console: ::

    ###!!! [Parent][RunMessage] Error: (msgtype=0x410001,name=PDemo::Msg_InitCrashReporter) Channel closing: too late to send/recv, messages will be lost

    [Parent 16672, IPC I/O Parent] WARNING: file c:/mozilla-src/mozilla-unified/ipc/chromium/src/base/process_util_win.cc:167
    [Parent 16672, Main Thread] WARNING: Not resolving response because actor is dead.: file c:/mozilla-src/mozilla-unified/ipc/glue/ProtocolUtils.cpp:931
    [Parent 16672, Main Thread] WARNING: IPDL resolver dropped without being called!: file c:/mozilla-src/mozilla-unified/ipc/glue/ProtocolUtils.cpp:959

We could probably figure out what is happening here from the messages but,
with more complex protocols, understanding what led to this may not be so easy.
To begin diagnosing, we can turn on IPC Logging, which was defined in the IPDL
section on :ref:`Message Logging`.  We just need to set an environment variable
before starting the browser.  Let's turn it on for all ``PDemo`` and
``PDemoHelpline`` actors: ::

    MOZ_IPC_MESSAGE_LOG="PDemoParent,PDemoChild,PDemoHelplineParent,PDemoHelplineChild"

To underscore what we said above, when logging is active, the change in timing
makes the error message go away and everything closes properly on a tested
Windows desktop.  However, the issue remains on a Macbook Pro and the log
shows the issue rather clearly: ::

    [time: 1627075553937959][63096->63085] [PDemoChild] Sending  PDemo::Msg_InitCrashReporter
    [time: 1627075553949441][63085->63096] [PDemoParent] Sending  PDemo::Msg_CreateDemoHelplineChild
    [time: 1627075553950293][63092->63096] [PDemoHelplineParent] Sending  PDemoHelpline::Msg_RequestAssistance
    [time: 1627075553979151][63096<-63085] [PDemoChild] Received  PDemo::Msg_CreateDemoHelplineChild
    [time: 1627075553979433][63096<-63092] [PDemoHelplineChild] Received  PDemoHelpline::Msg_RequestAssistance
    [time: 1627075553979498][63096->63092] [PDemoHelplineChild] Sending  PDemoHelpline::Msg_GiveAssistance
    [time: 1627075553980105][63092<-63096] [PDemoHelplineParent] Received  PDemoHelpline::Msg_GiveAssistance
    [time: 1627075553980181][63092->63096] [PDemoHelplineParent] Sending reply  PDemoHelpline::Reply_GiveAssistance
    [time: 1627075553980449][63096<-63092] [PDemoHelplineChild] Received  PDemoHelpline::Reply_GiveAssistance
    [tab 63092] NOTE: parent actor received `Goodbye' message.  Closing channel.
    [default 63085] NOTE: parent actor received `Goodbye' message.  Closing channel.
    [...]
    ###!!! [Parent][RunMessage] Error: (msgtype=0x420001,name=PDemo::Msg_InitCrashReporter) Channel closing: too late to send/recv, messages will be lost
    [...]
    [default 63085] NOTE: parent actor received `Goodbye' message.  Closing channel.

The imbalance with ``Msg_InitCrashReporter`` is clear.  The message was not
*Received* before the channel was closed.  Note that the first ``Goodbye`` for
the main (default) process is for the ``PDemoHelpline`` actor -- in this case,
its child actor was in a content (tab) process.  The second default process
``Goodbye`` is from the **Demo** process, sent when doing ``Close``.  It might
seem that it should handle the ``Msg_InitCrashReporter`` if it can handle the
later ``Goodbye`` but this does not happen for safety reasons.

Early Debugging For A New Process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Let's assume now that we still don't understand the problem -- maybe we don't
know that the ``InitCrashReporter`` message is sent internally by the
``CrashReporterClient`` we initialized.  Or maybe we're only looking at Windows
builds.  We decide we'd like to be able to hook a debugger to the new process
so that we can break on the ``SendInitCrashReporter`` call.  Attaching the
debugger has to happen fast -- process startup probably completes in under a
second.  Debugging this is not always easy.

Windows users have options that work with both the Visual Studio and WinDbg
debuggers.  For Visual Studio users, there is an easy-to-use VS addon called
the `Child Process Debugging Tool`_ that allows you to connect to *all*
processes that are launched by a process you are debugging.  So, if the VS
debugger is connected to the main process, it will automatically connect to the
new **Demo** process (and every other launched process) at the point that they
are spawned.  This way, the new process never does anything outside of the
debugger.  Breakpoints, etc work as expected.  The addon mostly works like a
toggle and will remain on until it is disabled from the VS menu.

WinDbg users can achieve essentially the same behavior with the `.childdbg`_
command.  See the docs for details but essentially all there is to know is that
``.childdbg 1`` enables it and ``.childdbg 0`` disables it.  You might add it
to a startup config file (see the WinDbg ``-c`` command line option)

Linux and mac users should reference gdb's ``detach-on-fork``.  The command to
debug child processes is ``set detach-on-fork off``.  Again, the behavior is
largely what you would expect -- that all spawned processes are added to the
current debug session.  The command can be added to ``.gdbinit`` for ease.  At
the time of this writing, lldb does not support automatically connecting to
newly spawned processes.

Finally, Linux users can use ``rr`` for time-travel debugging.  See `Debugging
Firefox with rr`_ for details.

These solutions are not always desirable.  For example, the fact that they hook
*all* spawned processes can mean that targeting breakpoints to one process
requires us to manually disconnect many other processes.  In these cases, an
easier solution may be to use Gecko environment variables that will cause the
process to sleep for some number of seconds.  During that time, you can find
the process ID (PID) for the process you want to debug and connect your
debugger to it.  OS tools like ``ProcessMonitor`` can give you the PID but it
will also be clearly logged to the console just before the process waits.

Set ``MOZ_DEBUG_CHILD_PROCESS=1`` to turn on process startup pausing.  You can
also set ``MOZ_DEBUG_CHILD_PAUSE=N`` where N is the number of seconds to sleep.
The default is 10 seconds on Windows and 30 on other platforms.

Pausing for the debugger is not a panacea.  Since the environmental varaiables
are not specific to process type, you will be forced to wait for all of the
processes Gecko creates before you wait for it to get to yours.  The pauses can
also end up exposing unknown concurrency bugs in the browser before it even
gets to your issue, which is good to discover but doesn't fix your bug.  That
said, any of these strategies would be enough to facilitate easily breaking on
``SendInitCrashReporter`` and finding our sender.

.. _Child Process Debugging Tool: https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool
.. _.childdbg: https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/-childdbg--debug-child-processes-
