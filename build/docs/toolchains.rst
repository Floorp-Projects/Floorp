.. _build_toolchains:

===========================
Creating Toolchain Archives
===========================

There are various scripts in the repository for producing archives
of the build tools (e.g. compilers and linkers) required to build.

Clang
=====

See the ``build/build-clang`` directory. Read ``build/build-clang/README``
for more.

Windows
=======

The ``build/windows_toolchain.py`` script is used to build and manage
Windows toolchain archives containing Visual Studio executables, SDKs,
etc.

The way Firefox build automation works is an archive containing the
toolchain is produced and uploaded to an internal Mozilla server. The
build automation will download, verify, and extract this archive before
building. The archive is self-contained so machines don't need to install
Visual Studio, SDKs, or various other dependencies. Unfortunately,
Microsoft's terms don't allow Mozilla to distribute this archive
publicly. However, the same tool can be used to create your own copy.

Configuring Your System
-----------------------

It is **highly** recommended to perform this process on a fresh installation
of Windows 7 or 10 (such as in a VM). Installing all updates through
Windows Update is not only acceptable - it is encouraged. Although it
shouldn't matter.

Next, install Visual Studio 2015 Community. The download link can be
found at https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx.
Be sure to follow these install instructions:

1. Choose a ``Custom`` installation and click ``Next``
2. Select ``Programming Languages`` -> ``Visual C++`` (make sure all sub items are
   selected)
3. Under ``Windows and Web Development`` uncheck everything except
   ``Universal Windows App Development Tools`` and the items under it
   (should be ``Tools (1.3.1)...`` and the ``Windows 10 SDK``).

Once Visual Studio 2015 Community has been installed, from a checkout
of mozilla-central, run something like the following to produce a ZIP
archive::

   $ ./mach python build/windows_toolchain.py create-zip vs2015u2

The produced archive will be the argument to ``create-zip`` + ``.zip``.

Firefox for Android with Gradle
===============================

To build Firefox for Android with Gradle in automation, archives
containing both the Gradle executable and a Maven repository
comprising the exact build dependencies are produced and uploaded to
an internal Mozilla server.  The build automation will download,
verify, and extract these archive before building.  These archives
provide a self-contained Gradle and Maven repository so that machines
don't need to fetch additional Maven dependencies at build time.
(Gradle and the downloaded Maven dependencies can be both
redistributed publicly.)

Archiving the Gradle executable is straight-forward, but archiving a
local Maven repository is not.  Therefore a special Task Cluster
Docker image and job exist for producing the required archives.  The
Docker image definition is rooted in
``taskcluster/docker/android-gradle-build``.  The Task Cluster job
definition is in
``testing/taskcluster/tasks/builds/android_api_15_gradle_dependencies.yml``.
The job runs in a container based on the custom Docker image and
spawns a Sonatype Nexus proxying Maven repository process in the
background.  The job builds Firefox for Android using Gradle and the
in-tree Gradle configuration rooted at ``build.gradle``.  The spawned
proxying Maven repository downloads external dependencies and collects
them.  After the Gradle build completes, the job archives the Gradle
version used to build, and the downloaded Maven repository, and
exposes them as Task Cluster artifacts.

Here is `an example try job fetching these dependencies
<https://treeherder.mozilla.org/#/jobs?repo=try&revision=75bc98935147&selectedJob=17793653>`_.
The resulting task produced a `Gradle archive
<https://queue.taskcluster.net/v1/task/CeYMgAP3Q-KF8h37nMhJjg/runs/0/artifacts/public%2Fbuild%2Fgradle.tar.xz>`_
and a `Maven repository archive
<https://queue.taskcluster.net/v1/task/CeYMgAP3Q-KF8h37nMhJjg/runs/0/artifacts/public%2Fbuild%2Fjcentral.tar.xz>`_.
These archives were then uploaded (manually) to Mozilla automation
using tooltool for consumption in Gradle builds.

To update the version of Gradle in the archive produced, update
``gradle/wrapper/gradle-wrapper.properties``.  Be sure to also update
the SHA256 checksum to prevent poisoning the build machines!

To update the versions of Gradle dependencies used, update
``dependencies`` sections in the in-tree Gradle configuration rooted
at ``build.gradle``.  Once you are confident your changes build
locally, push a fresh try build with an invocation like::

   $ hg push-to-try -m "try: -b o -p android-api-15-gradle-dependencies"

Then `upload your archives to tooltool
<https://wiki.mozilla.org/ReleaseEngineering/Applications/Tooltool#How_To_Upload_To_Tooltool>`_,
update the in-tree manifests in
``mobile/android/config/tooltool-manifests``, and push a fresh try
build.
