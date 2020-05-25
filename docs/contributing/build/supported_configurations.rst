Supported Firefox Build Targets
===============================

There are three tiers of **supported Firefox build targets** at this
time. These tiers represent the shared engineering priorities of the
Mozilla project.

The term **"Tier-1 platform"** refers to those platforms - CPU
architectures and operating systems - that are the primary focus of
Firefox development efforts. Tier-1 platforms are fully supported by
Mozilla's `continuous integration processes <https://developer.mozilla.org/en-US/docs/>` and the
`Try Server <https://developer.mozilla.org/en-US/docs/>`. Any proposed change to Firefox on these
platforms that results in build failures, test failures, performance
regressions or other major problems **will be reverted immediately**.

Tier-1 Firefox Platforms
------------------------

The **Tier-1 Firefox platforms** and their supported compilers are:

-  Android on Linux x86, x86-64, ARMv7 and ARMv8-A (clang)
-  Linux/x86 and x86-64 (gcc and clang)
-  OSX 10.9 and later on x86-64 (clang)
-  Windows/x86, x86-64 and AArch64 (clang-cl)

Prior to Firefox 63, Windows/x86 and Windows/x86-64 relied on the MSVC
compiler; from **Firefox 63 onward MSVC is not supported**. Older 32-bit
x86 CPUs without SSE2 instructions such as the Pentium III and Athlon XP
are also **not considered Tier-1 platforms, and are not supported**.
Note also that while Windows/x86 and ARM/AArch64 are supported *as build
targets*, it is not possible to build Firefox *on* Windows/x86 or
Windows/AArch64 systems.

Tier-2 Firefox Platforms
------------------------

**Tier-2 platforms** are actively maintained by the Mozilla community,
though with less rigorous requirements. Proposed changes resulting in
breakage or regressions limited to these platforms **may not immediately
result in reversion**. However, developers who break these platforms are
expected to work with platform maintainers to fix problems, and **may be
required to revert their changes** if a fix cannot be found.

The **Tier-2 Firefox platforms** and their supported compilers are:

-  Linux/AArch64 (clang)
-  Windows/x86 (mingw-clang) - maintained by Tom Ritter and Jacek Caban

.. note:

*Note that some features of this platform are disabled, as they
require MS COM or the w32api project doesn't expose the necessary
Windows APIs.*

Tier-3 Firefox Platforms
------------------------

**Tier-3 platforms** have a maintainer or community which attempt to
keep the platform working. These platforms are **not supported by our
continuous integration processes**, and **Mozilla does not routinely
test on these platforms**, nor do we block further development on the
outcomes of those tests.

At any given time a Firefox built from mozilla-central for these
platforms may or may not work correctly or build at all.

**Tier-3 Firefox platforms** include: 

-  Linux on various CPU architectures including ARM variants not listed
   above, PowerPC, and x86 CPUs without SSE2 support - maintained by
   various Linux distributions
-  FreeBSD/x86, x86-64, Aarch64 (clang) - maintained by Jan Beich
-  OpenBSD/x86, x86-64 (clang) - maintained by Landry Breuil
-  NetBSD/x86-64 (gcc) - maintained by David Laight
-  Solaris/x86-64, sparc64 (gcc) - maintained by Petr Sumbera
-  [STRIKEOUT:Windows/x86-64 (mingw-gcc)] - Unsupported due to
   requirements for clang-bindgen

If you're filing a bug against Firefox on a Tier-3 platform (or any
combination of OS, CPU and compiler not listed above) please bear in
mind that Mozilla developers do not reliably have access to non-Tier-1
platforms or build environments. To be actionable bug reports against
non-Tier-1 platforms should include as much information as possible to
help the owner of the bug determine the cause of the problem and the
proper solution. If you can provide a patch, a regression range or
assist in verifying that the developer's patches work for your platform,
that would help a lot towards getting your bugs fixed and checked into
the tree.
