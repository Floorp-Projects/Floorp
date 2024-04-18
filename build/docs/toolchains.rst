.. _build_toolchains:

===========================
Creating Toolchain Archives
===========================

There are various scripts in the repository for producing archives
of the build tools (e.g. compilers and linkers) required to build.

Clang and Rust
==============

To modify the toolchains used for a particular task, you may need several
things:

1. A `build task`_

2. Which uses a toolchain task

    - `clang toolchain`_
    - `rust toolchain`_

3. Which uses a git fetch

    - `clang fetch`_
    - (from-source ``dev`` builds only) `rust fetch`_

4. (clang only) Which uses a `config json`_

5. Which takes patches_ you may want to apply.

For the most part, you should be able to accomplish what you want by
copying/editing the existing examples in those files.

.. _build task: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/ci/build/linux.yml#5-45
.. _clang toolchain: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/ci/toolchain/clang.yml#51-72
.. _rust toolchain: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/ci/toolchain/rust.yml#57-74
.. _clang fetch: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/ci/fetch/toolchains.yml#413-418
.. _rust fetch: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/ci/fetch/toolchains.yml#434-439
.. _config json: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/build/build-clang/clang-linux64.json
.. _patches: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/build/build-clang/static-llvm-symbolizer.patch

Clang
-----

Building clang is handled by `build-clang.py`_, which uses several resources
in the `build-clang`_ directory. Read the `build-clang README`_ for more
details.

Note for local builds: build-clang.py can be run on developer machines but its
lengthy multi-stage build process is unnecessary for most local development. The
upstream `LLVM Getting Started Guide`_ has instructions on how to build
clang more directly.

.. _build-clang.py: https://searchfox.org/mozilla-central/source/build/build-clang/build-clang.py
.. _build-clang README: https://searchfox.org/mozilla-central/source/build/build-clang/README
.. _build-clang: https://searchfox.org/mozilla-central/source/build/build-clang/
.. _LLVM Getting Started Guide: https://llvm.org/docs/GettingStarted.html

Rust
----

Rust builds are handled by `repack_rust.py`_. The primary purpose of
that script is to download prebuilt tarballs from the Rust project.

It uses the same basic format as `rustup` for specifying the toolchain
(via ``--channel``):

- request a stable build with ``1.xx.y`` (e.g. ``1.47.0``)
- request a beta build with ``beta-yyyy-mm-dd`` (e.g. ``beta-2020-08-26``)
- request a nightly build with ``nightly-yyyy-mm-dd`` (e.g. ``nightly-2020-08-26``)
- request a build from `Rust's ci`_ with ``bors-$sha`` (e.g. ``bors-796a2a9bbe7614610bd67d4cd0cf0dfff0468778``)
- request a from-source build with ``dev``

Rust From Source
----------------

As of this writing, from-source builds for Rust are a new feature, and not
used anywhere by default. The feature was added so that we can test patches
to rustc against the tree. Expect things to be a bit hacky and limited.

Most importantly, building from source requires your toolchain to have a
`fetch of the rust tree`_ as well as `clang and binutils toolchains`_. It is also
recommended to upgrade the worker-type to e.g. ``b-linux-large``.

Rust's build dependencies are fairly minimal, and it has a sanity check
that should catch any missing or too-old dependencies. See the `Rust README`_
for more details.

Patches are set via `the --patch flag`_ (passed via ``toolchain/rust.yml``).
Patch paths are assumed to be relative to ``/build/build-rust/``, and may be
optionally prefixed with ``module-path:`` to specify they apply to that git
submodule in the Rust source. e.g. ``--patch src/llvm-project:mypatch.diff``
patches rust's llvm with ``/build/build-rust/mypatch.diff``. There are no
currently checked in rust patches to use as an example, but they should be
the same format as `the clang ones`_.

Rust builds are not currently configurable, and uses a `hardcoded config.toml`_,
which you may need to edit for your purposes. See Rust's `example config`_ for
details/defaults. Note that these options do occasionally change, so be sure
you're using options for the version you're targeting. For instance, there was
a large change around Rust ~1.48, and the currently checked in config was for
1.47, so it may not work properly when building the latest version of Rust.

Rust builds are currently limited to targeting only the host platform.
Although the machinery is in place to request additional targets, the
cross-compilation fails for some unknown reason. We have not yet investigated
what needs to be done to get this working.

While Rust generally maintains a clean tree for building ``rustc`` and
``cargo``, other tools like ``rustfmt`` or ``miri`` are allowed to be
transiently broken. This means not every commit in the Rust tree will be
able to build the `tools we require`_.

Although ``repack_rust`` considers ``rustfmt`` an optional package, Rust builds
do not currently implement this and will fail if ``rustfmt`` is busted. Some
attempt was made to work around it, but `more work is needed`_.

.. _Rust's ci: https://github.com/rust-lang/rust/pull/77875#issuecomment-736092083
.. _repack_rust.py: https://searchfox.org/mozilla-central/source/taskcluster/scripts/misc/repack_rust.py
.. _fetch of the rust tree: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/ci/toolchain/rust.yml#69-71
.. _clang and binutils toolchains: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/ci/toolchain/rust.yml#72-74
.. _the --patch flag: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/scripts/misc/repack_rust.py#667-675
.. _the clang ones: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/build/build-clang/static-llvm-symbolizer.patch
.. _Rust README: https://github.com/rust-lang/rust/#building-on-a-unix-like-system
.. _hardcoded config.toml: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/scripts/misc/repack_rust.py#384-421
.. _example config: https://github.com/rust-lang/rust/blob/b7ebc6b0c1ba3c27ebb17c0b496ece778ef11e18/config.toml.example
.. _tools we require: https://searchfox.org/mozilla-central/rev/168c45a7acc44e9904cfd4eebcb9eb080e05699c/taskcluster/scripts/misc/repack_rust.py#398
.. _more work is needed: https://github.com/rust-lang/rust/issues/79249

Python
------

Python is built from source by ``taskcluster/scripts/misc/build-cpython.sh`` on
Linux and OSX. We use the upstream installer on Windows, through
``taskcluster/scripts/misc/pack-cpython.sh``. In order to ensure consistency, we
use the same version for both approaches. Note however that the Windows installer is
not packaged for all patch versions, so there might be a slight delta there.

Windows
=======

The ``build/vs/generate_yaml.py`` and ``taskcluster/scripts/misc/get_vs.py``
scripts are used to manage and get Windows toolchains containing Visual
Studio executables, SDKs, etc.

The ``build/vs/generate_yaml.py`` script is used to generate one of the
YAML files used in the relevant toolchain task. The exact command line
used to generate the file is stored in the header of the YAML file itself.
Each YAML file records the necessary downloads from Microsoft servers to
install the required Visual Studio components given on the command line.

The ``taskcluster/scripts/misc/get_vs.py`` script takes a YAML file as
input and fills a directory with the corresponding Visual Studio components.

Both scripts should be run via ``mach python --virtualenv build``. The
latter is automatically invoked by the bootstrapping mechanism.


MacOS
=====

The ``build/macosx/catalog.py`` and ``taskcluster/scripts/misc/unpack-sdk.py``
scripts are used to manage and get macOS SDKs.

The ``build/macosx/catalog.py`` script is used to explore the Apple
software update catalog. Running the script with no argument will show
a complete list of "products". You probably don't want that, but rather
start with a filter:

.. code-block:: shell

    $ ./mach python build/macosx/catalog.py --filter SDK
    061-44071 Beats Updater 1.0
    071-29699 Command Line Tools for Xcode 12.5
    001-89745 Command Line Tools for Xcode 12.4
    071-54303 Command Line Tools for Xcode 12.5
    002-41708 Command Line Tools for Xcode 13.2
    002-83793 Command Line Tools for Xcode 13.4
    012-92431 Command Line Tools for Xcode 14.2
    032-64167 Command Line Tools for Xcode 14.3

From there, pick the id of the product you're interested in, and run the
script again with that id:

.. code-block:: shell

    $ ./mach python build/macosx/catalog.py 032-64167
    com.apple.pkg.CLTools_Executables https://swcdn.apple.com/content/downloads/38/61/032-64167-A_F8LL7XSTW6/k3kg0uip4kxd3qupgy6y8fzp27mnxdpt6y/CLTools_Executables.pkg
    com.apple.pkg.CLTools_SDK_macOS13 https://swcdn.apple.com/content/downloads/38/61/032-64167-A_F8LL7XSTW6/k3kg0uip4kxd3qupgy6y8fzp27mnxdpt6y/CLTools_macOSNMOS_SDK.pkg
    com.apple.pkg.CLTools_SDK_macOS12 https://swcdn.apple.com/content/downloads/38/61/032-64167-A_F8LL7XSTW6/k3kg0uip4kxd3qupgy6y8fzp27mnxdpt6y/CLTools_macOSLMOS_SDK.pkg
    com.apple.pkg.CLTools_macOS_SDK https://swcdn.apple.com/content/downloads/38/61/032-64167-A_F8LL7XSTW6/k3kg0uip4kxd3qupgy6y8fzp27mnxdpt6y/CLTools_macOS_SDK.pkg
    com.apple.pkg.CLTools_SwiftBackDeploy https://swcdn.apple.com/content/downloads/38/61/032-64167-A_F8LL7XSTW6/k3kg0uip4kxd3qupgy6y8fzp27mnxdpt6y/CLTools_SwiftBackDeploy.pkg

From there, pick the id of the package you're interested in, and run the
script again with a combination of both product and package ids to inspect
its content and ensure that's what you're looking for.

.. code-block:: shell

    $ ./mach python build/macosx/catalog.py 032-64167/com.apple.pkg.CLTools_SDK_macOS13
    Library
    Library/Developer
    Library/Developer/CommandLineTools
    Library/Developer/CommandLineTools/SDKs
    Library/Developer/CommandLineTools/SDKs/MacOSX13.sdk
    Library/Developer/CommandLineTools/SDKs/MacOSX13.3.sdk
    Library/Developer/CommandLineTools/SDKs/MacOSX13.3.sdk/usr
    (...)

Once you have found the SDK you want, you can create or update toolchain tasks
in ``taskcluster/ci/toolchain/macosx-sdk.yml``.

The ``taskcluster/scripts/misc/unpack-sdk.py`` script takes the url of a SDK
package, the sha512 hash for its content, the path to the SDK in the package,
and an output directory, and extracts the package in that directory.

Both scripts should be run via ``mach python``. The latter is automatically
invoked by the bootstrapping mechanism.

On automation, the script will download the file from tooltool instead of the
original url, so the file should also be uploaded to tooltool with `internal`
visibility.
See https://github.com/mozilla-releng/tooltool.

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
local Maven repository is not.  Therefore a toolchain job exists for
producing the required archives, `android-gradle-dependencies`.  The
job runs in a container based on a custom Docker image and spawns a
Sonatype Nexus proxying Maven repository process in the background.
The job builds Firefox for Android using Gradle and the in-tree Gradle
configuration rooted at ``build.gradle``.  The spawned proxying Maven
repository downloads external dependencies and collects them.  After
the Gradle build completes, the job archives the Gradle version used
to build, and the downloaded Maven repository, and exposes them as
Task Cluster artifacts.

To update the version of Gradle in the archive produced, update
``gradle/wrapper/gradle-wrapper.properties``.  Be sure to also update
the SHA256 checksum to prevent poisoning the build machines!

To update the versions of Gradle dependencies used, update
``dependencies`` sections in the in-tree Gradle configuration rooted
at ``build.gradle``.  Once you are confident your changes build
locally, push a fresh build to try.  The `android-gradle-dependencies`
toolchain should run automatically, fetching your new dependencies and
wiring them into the appropriate try build jobs.

To update the version of Sonatype Nexus, update the `sonatype-nexus`
`fetch` task definition.

To modify the Sonatype Nexus configuration, typically to proxy a new
remote Maven repository, modify
`taskcluster/scripts/misc/android-gradle-dependencies/nexus.xml`.

There is also a toolchain job that fetches the Android SDK and related
packages.  To update the versions of packaged fetched, modify
`python/mozboot/mozboot/android-packages.txt` and update the various
in-tree versions accordingly.
