Process Model
=============

The complete set of recognized process types is defined in `GeckoProcessTypes.h <https://searchfox.org/mozilla-central/source/xpcom/build/GeckoProcessTypes.h>`_.

For more details on how process types are added and managed by IPC, see the process creation documentation. (FIXME: being added in `<https://phabricator.services.mozilla.com/D121871>`_)

Diagram
-------

.. digraph:: processtypes
    :caption: Diagram of processes used by Firefox. All child processes are spawned and managed by the Parent process.

    compound=true;
    node [shape=rectangle];

    launcher [label=<Launcher Process>]
    parent [label=<Parent Process>]

    subgraph cluster_child {
        color=lightgrey;
        label=<Child Processes>;

        subgraph cluster_content {
            color=lightgrey;
            label=<Content Processes>;

            web [
                color=lightgrey;
                label=<
                    <TABLE BORDER="0" CELLSPACING="5" CELLPADDING="5" COLOR="black">
                        <TR><TD BORDER="0" CELLPADDING="0" CELLSPACING="0">Web Content</TD></TR>
                        <TR><TD BORDER="1">Shared Web Content<BR/>(<FONT FACE="monospace">web</FONT>)</TD></TR>
                        <TR><TD BORDER="1">Isolated Web Content<BR/>(<FONT FACE="monospace">webIsolated=$SITE</FONT>)</TD></TR>
                        <TR><TD BORDER="1">COOP+COEP Web Content<BR/>(<FONT FACE="monospace">webCOOP+COEP=$SITE</FONT>)</TD></TR>
                        <TR><TD BORDER="1">Large Allocation Web Content<BR/>(<FONT FACE="monospace">webLargeAlloc</FONT>)</TD></TR>
                    </TABLE>
                >
            ]

            nonweb [
                shape=none;
                label=<
                    <TABLE BORDER="0" CELLSPACING="5" CELLPADDING="5" COLOR="black">
                        <TR><TD BORDER="1">Preallocated Content<BR/>(<FONT FACE="monospace">prealloc</FONT>)</TD></TR>
                        <TR><TD BORDER="1">File Content<BR/>(<FONT FACE="monospace">file</FONT>)</TD></TR>
                        <TR><TD BORDER="1">WebExtensions<BR/>(<FONT FACE="monospace">extension</FONT>)</TD></TR>
                        <TR><TD BORDER="1">Privileged Content<BR/>(<FONT FACE="monospace">privilegedabout</FONT>)</TD></TR>
                        <TR><TD BORDER="1">Privileged Mozilla Content<BR/>(<FONT FACE="monospace">privilegedmozilla</FONT>)</TD></TR>
                    </TABLE>
                >
            ]
        }

        helper [
            color=lightgrey;
            label=<
                <TABLE BORDER="0" CELLSPACING="5" CELLPADDING="5" COLOR="black">
                    <TR><TD BORDER="0" CELLPADDING="0" CELLSPACING="0">Helper Processes</TD></TR>
                    <TR><TD BORDER="1">Gecko Media Plugins (GMP) Process</TD></TR>
                    <TR><TD BORDER="1">GPU Process</TD></TR>
                    <TR><TD BORDER="1">VR Process</TD></TR>
                    <TR><TD BORDER="1">Data Decoder (RDD) Process</TD></TR>
                    <TR><TD BORDER="1">Network (Socket) Process</TD></TR>
                    <TR><TD BORDER="1">Remote Sandbox Broker Process</TD></TR>
                    <TR><TD BORDER="1">Fork Server</TD></TR>
                </TABLE>
            >
        ]
    }

    subgraph { rank=same; launcher -> parent; }

    parent -> web [lhead="cluster_content"];
    parent -> helper;


Parent Process
--------------

:remoteType: *null*
:other names: UI Process, Main Process, Chrome Process, Browser Process, Default Process, Broker Process
:sandboxed?: no

The parent process is the primary process which handles the core functionality of Firefox, including its UI, profiles, process selection, navigation, and more. The parent process is responsible for launching all other child processes, and acts as a broker establishing communication between them.

All primary protocols establish a connection between the parent process and the given child process, which can then be used to establish additional connections to other processes.

As the parent process can display HTML and JS, such as the browser UI and privileged internal pages such as ``about:preferences`` and ``about:config``, it is often treated as-if it was a content process with a *null* remote type by process selection logic. The parent process has extra protections in place to ensure it cannot load untrusted code when running in multiprocess mode. To this effect, any attempts to load web content in the parent process will lead to a browser crash, and all navigations to and from parent-process documents immediately perform full isolation, to prevent content processes from manipulating them.

Content Process
---------------

:primary protocol: `PContent <https://searchfox.org/mozilla-central/source/dom/ipc/PContent.ipdl>`_
:other names: Renderer Process
:sandboxed?: yes (content sandbox policy)

Content processes are used to load web content, and are the only process type (other than the parent process) which can load and execute JS code. These processes are further subdivided into specific "remote types", which specify the type of content loaded within them, their sandboxing behavior, and can gate access to certain privileged IPC methods.

The specific remote type and isolation behaviour used for a specific resource is currently controlled in 2 major places. When performing a document navigation, the final process to load the document in is selected by the logic in `ProcessIsolation.cpp <https://searchfox.org/mozilla-central/source/dom/ipc/ProcessIsolation.cpp>`_. This will combine information about the specific response, such as the site and headers, with other state to select which process and other isolating actions should be taken. When selecting which process to create the initial process for a new tab in, and when selecting processes for serviceworkers and shared workers, the logic in `E10SUtils.jsm <https://searchfox.org/mozilla-central/source/toolkit/modules/E10SUtils.jsm>`_ is used to select a process. The logic in ``E10SUtils.jsm`` will likely be removed and replaced with ``ProcessIsolation.cpp`` in the future.

.. note::

    The "Renderer" alternative name is used by Chromium for its equivalent to content processes, and is occasionally used in Gecko as well, due to the similarity in process architecture. The actual rendering & compositing steps are performed in the GPU or main process.

Preallocated Content
^^^^^^^^^^^^^^^^^^^^

:remoteType: ``prealloc``
:default count: 3 (``dom.ipc.processPrelaunch.fission.number``, or 1 if Fission is disabled)

To avoid the need to launch new content processes to host new content when navigating, new content processes are pre-launched and specialized when they are requested. These preallocated content processes will never load content, and must be specialized before they can be used.

The count of preallocated processes can vary depending on various factors, such as the memory available in the host system.

The ``prealloc`` process cannot be used to launch ``file`` content processes, due to their weakened OS sandbox. ``extension`` content processes are also currently not supported due to `Bug 1637119 <https://bugzilla.mozilla.org/show_bug.cgi?id=1638119>`_.

File Content
^^^^^^^^^^^^

:remoteType: ``file``
:default count: 1 (``dom.ipc.processCount.file``)
:capabilities: File System Access

The File content process is used to load ``file://`` URIs, and is therefore less sandboxed than other content processes. It may also be used to load remote web content if the browser has used a legacy CAPS preference to allow that site to access local resources (see `Bug 995943 <https://bugzilla.mozilla.org/show_bug.cgi?id=995943>`_)

WebExtensions
^^^^^^^^^^^^^

:remoteType: ``extension``
:default count: 1 (``dom.ipc.processCount.extension``)
:capabilities: Extension APIs, Shared Memory (SharedArrayBuffer)

The WebExtension content process is used to load background pages and top level WebExtension frames. This process generally has access to elevated permissions due to loading privileged extension pages with access to the full WebExtension API surface. Currently all extensions share a single content process.

Privileged extensions loaded within the extension process may also be granted access to shared memory using SharedArrayBuffer.

.. note::

    ``moz-extension://`` subframes are currently loaded in the same process as the parent document, rather than in the ``extension`` content process, due to existing permissions behaviour granting content scripts the ability to access the content of extension subframes. This may change in the future.

Privileged Content
^^^^^^^^^^^^^^^^^^

:remoteType: ``privilegedabout``
:default count: 1 (``dom.ipc.processCount.privilegedabout``)
:capabilities: Restricted JSWindowActor APIs

The ``privilegedabout`` content process is used to load internal pages which have privileged access to internal state. The use of the ``privilegedabout`` content process is requested by including both ``nsIAboutModule::URI_MUST_LOAD_IN_CHILD`` and ``nsIAboutModule::URI_CAN_LOAD_IN_PRIVILEGEDABOUT_PROCESS`` flags in the corresponding ``nsIAboutModule``.

As of August 11, 2021, the following internal pages load in the privileged content process: ``about:logins``, ``about:loginsimportreport``, ``about:privatebrowsing``, ``about:home``, ``about:newtab``, ``about:welcome``, ``about:protections``, and ``about:certificate``.

Various ``JSWindowActor`` instances which provide special API access for these internal about pages are restricted to only be available in this content process through the ``remoteTypes`` attribute, which will block attempts to use them from other content processes.

Privileged Mozilla Content
^^^^^^^^^^^^^^^^^^^^^^^^^^

:remoteType: ``privilegedmozilla``
:default count: 1 (``dom.ipc.processCount.privilegedmozilla``)
:domains: ``addons.mozilla.org`` and ``accounts.firefox.com`` (``browser.tabs.remote.separatedMozillaDomains``)
:capabilities: Restricted Addon Manager APIs

The ``privilegedmozilla`` content process is used to load specific high-value Mozilla-controlled webpages which have been granted access to privileged features. To provide an extra layer of security for these sites, they are loaded in a separate process from other web content even when Fission is disabled.

This separate remote type is also used to gate access at the IPC boundary to certain high-power web APIs, such as access to the ability to interact with installed extension APIs.

Web Content Processes
^^^^^^^^^^^^^^^^^^^^^

These processes all have remote types beginning with ``web``, and are used to host general untrusted web content. The different variants of web content processes are used at different times, depending on the isolation strategy requested by the page and the browser's configuration.

Shared Web Content
""""""""""""""""""

:remoteType: ``web``
:default count: 8 (``dom.ipc.processCount``)

The shared web content process is used to host content which is not isolated into one of the other web content process types. This includes almost all web content with Fission disabled, and web content which cannot be attributed to a specific origin with Fission enabled, such as user-initiated ``data:`` URI loads.

Isolated Web Content
""""""""""""""""""""

:remoteType: ``webIsolated=$SITE``
:default count: 1 per-site (``dom.ipc.processCount.webIsolated``)

Isolated web content processes are used to host web content with Fission which can be attributed to a specific site. These processes are allocated when navigating, and will only load content from the named site. When Fission is disabled, isolated web content processes are not used.

A different ``webIsolated=`` remote type, and therefore a different pool of processes, is used for each site loaded, with separation also being used for different container tabs and private browsing.

COOP+COEP Web Content
"""""""""""""""""""""

:remoteType: ``webCOOP+COEP=$SITE``
:default count: 1 per-site (``dom.ipc.processCount.webCOOP+COEP``)
:capabilities: Shared Memory (SharedArrayBuffer)

When loading a top level document with both the ``Cross-Origin-Opener-Policy`` and ``Cross-Origin-Embedder-Policy`` headers configured correctly, the document is requesting access to Shared Memory. For security reasons, we only provide this API access to sufficiently-isolated pages, and we load them within special isolated content processes.

Like Isolated Web Content, these processes are keyed by the site loaded within them, and are also segmented based on container tabs and private browsing.

.. note::

    Another name for this process may be "Cross-Origin Isolated Web Content", to correspond with the ``window.crossOriginIsolated`` attribute which is set for documents loaded with these headers set. Unfortunately that may be confused with Fission's "Isolated Web Content" processes, as the attribute was named after the ``webIsolated`` remote type was already in use.

    In ``about:processes``, COOP+COEP Web Content processes will be listed with a "cross-origin isolated" note after the PID, like ``https://example.com (12345, cross-origin isolated)``.

Large Allocation Web Content
""""""""""""""""""""""""""""

:remoteType: ``webLargeAlloc``
:default count: 10 (``dom.ipc.processCount.webLargeAlloc``)
:platform: 32-bit Windows only (``dom.largeAllocation.forceEnable``)

Document loads with the non-standard ``Large-Allocation`` header are requesting to be placed into a separate content process such that they can have access to a less-fragmented address space. This was originally designed to enable 32-bit Windows platforms to load and run asm.js and wasm code more easily.

This header is only supported on 32-bit Windows, and will likely be removed in the near future.

Gecko Media Plugins (GMP) Process
---------------------------------

:primary protocol: `PGMP <https://searchfox.org/mozilla-central/source/dom/media/gmp/PGMP.ipdl>`_
:sandboxed?: yes (GMP sandbox policy)

The GMP process is used to sandbox third-party "Content Decryption Module" (CDM) binaries used for media playback in a sandboxed environment. This process is only launched when DRM-enabled content is loaded.

GPU Process
-----------

:primary protocol: `PGPU <https://searchfox.org/mozilla-central/source/gfx/ipc/PGPU.ipdl>`_
:other names: Compositor Process
:sandboxed?: no (`bug 1347710 <https://bugzilla.mozilla.org/show_bug.cgi?id=1347710>`_ tracks sandboxing on windows)

The GPU process performs compositing, and is used to talk to GPU hardware in an isolated process. This helps isolate things like GPU driver crashes from impacting the entire browser, and will allow for this code to be sandboxed in the future. In addition, some components like Windows Media Foundation (WMF) are run in the GPU process when it is available.

The GPU process is not used on all platforms. Platforms which do not use it, such as macOS and some Linux configurations, will perform compositing on a background thread in the Parent Process.

VR Process
----------

:primary protocol: `PVR <https://searchfox.org/mozilla-central/source/gfx/vr/ipc/PVR.ipdl>`_
:sandboxed?: no (`bug 1430043 <https://bugzilla.mozilla.org/show_bug.cgi?id=1430043>`_ tracks sandboxing on windows)

VR headset libraries require access to specific OS level features and other requirements which we would generally like to block with the sandbox in other processes. In order to allow the GPU process to have tighter sandboxing rules, these VR libraries are loaded into the less-restricted VR process. Like the GPU process, this serves to isolate them from the rest of Firefox and reduce the impact of bugs in these libraries on the rest of the browser. The VR process is launched only after a user visits a site which uses WebVR.

Data Decoder (RDD) Process
--------------------------

:primary protocol: `PRDD <https://searchfox.org/mozilla-central/source/dom/media/ipc/PRDD.ipdl>`_
:sandboxed?: yes (RDD sandbox policy)

This process is used to run media data decoders within their own sandboxed process, allowing the code to be isolated from other code in Gecko. This aims to reduce the severity of potential bugs in media decoder libraries, and improve the security of the browser.

.. note::

    This process is in the process of being restructured into a generic "utility" process type for running untrusted code in a maximally secure sandbox. After these changes, the following new process types will exist, replacing the RDD process:

    * ``Utility``: A maximally sandboxed process used to host untrusted code which does not require access to OS resources. This process will be even more sandboxed than RDD today on Windows, where the RDD process has access to Win32k.
    * ``UtilityWithWin32k``: A Windows-only process with the same sandboxing as the RDD process today. This will be used to host untrusted sandboxed code which requires access to Win32k to allow decoding directly into GPU surfaces.
    * ``GPUFallback``: A Windows-only process using the GPU process' sandboxing policy which will be used to run Windows Media Foundation (WMF) when the GPU process itself is unavailable, allowing ``UtilityWithWin32k`` to re-enable Arbitrary Code Guard (ACG) on Windows.

    For more details about the planned utility process architecture changes, see `the planning document <https://docs.google.com/document/d/1WDEY5fQetK_YE5oxGxXK9BzC1A8kJP3q6F1gAPc2UGE>`_.

Network (Socket) Process
------------------------

:primary protocol: `PSocketProcess <https://searchfox.org/mozilla-central/source/netwerk/ipc/PSocketProcess.ipdl>`_
:sandboxed?: yes (socket sandbox policy)

The socket process is used to separate certain networking operations from the parent process, allowing them to be performed more directly in a partially sandboxed process. The eventual goal is to move all TCP/UDP network operations into this dedicated process, and is being tracked in `Bug 1322426 <https://bugzilla.mozilla.org/show_bug.cgi?id=1322426>`_.

Remote Sandbox Broker Process
-----------------------------

:platform: Windows on ARM only
:primary protocol: `PRemoteSandboxBroker <https://searchfox.org/mozilla-central/source/security/sandbox/win/src/remotesandboxbroker/PRemoteSandboxBroker.ipdl>`_
:sandboxed?: no

In order to run sandboxed x86 plugin processes from Windows-on-ARM, the remote sandbox broker process is launched in x86-mode, and used to launch sandboxed x86 subprocesses. This avoids issues with the sandboxing layer, which unfortunately assumes that pointer width matches between the sandboxer and sandboxing process. To avoid this, the remote sandbox broker is used as an x86 sandboxing process which wraps these plugins.

Fork Server
-----------

:platform: Linux only
:pref: ``dom.ipc.forkserver.enable`` (disabled by default)
:primary protocol: *none*
:sandboxed?: no (processes forked by the fork server are sandboxed)

The fork server process is used to reduce the memory overhead and improve launch efficiency for new processes. When a new supported process is requested and the feature is enabled, the parent process will ask the fork server to ``fork(2)`` itself, and then begin executing. This avoids the need to re-load ``libxul.so`` and re-perform relocations.

The fork server must run before having initialized XPCOM or the IPC layer, and therefore uses a custom low-level IPC system called ``MiniTransceiver`` rather than IPDL to communicate.

Launcher Process
----------------

:platform: Windows only
:metabug: `Bug 1435780 <https://bugzilla.mozilla.org/show_bug.cgi?id=1435780>`_
:sandboxed?: no

The launcher process is used to bootstrap Firefox on Windows before launching the main Firefox process, allowing things like DLL injection blocking to initialize before the main thread even starts running, and improving stability. Unlike the other utility processes, this process is not launched by the parent process, but rather launches it.

IPDLUnitTest
------------

:primary protocol: varies

This test-only process type is intended for use when writing IPDL unit tests. However, it is currently broken, due to these tests having never been run in CI. The type may be removed or re-used when these unit tests are fixed.
