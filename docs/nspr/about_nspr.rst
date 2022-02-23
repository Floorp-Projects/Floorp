About NSPR
==========

NetScape Portable Runtime (NSPR) provides platform independence for
non-GUI operating system facilities. These facilities include threads,
thread synchronization, normal file and network I/O, interval timing and
calendar time, basic memory management (malloc and free) and shared
library linking.

History
~~~~~~~

A good portion of the library's purpose, and perhaps the primary purpose
in the Gromit environment, was to provide the underpinnings of the Java
VM, more or less mapping the *sys layer* that Sun defined for the
porting of the Java VM to various platforms. NSPR went beyond that
requirement in some areas and since it was also the platform independent
layer for most of the servers produced by Netscape. It was expected and
preferred that existing code be restructured and perhaps even rewritten
in order to use the NSPR API. It is not a goal to provide a platform for
the porting into Netscape of externally developed code.

At the time of writing the current generation of NSPR was known as
NSPR20. The first generation of NSPR was originally conceived just to
satisfy the requirements of porting Java to various host environments.
NSPR20, an effort started in 1996, built on that original idea, though
very little is left of the original code. (The "20" in "NSPR20" does not
mean "version 2.0" but rather "second generation".) Many of the concepts
have been reformed, expanded, and matured. Today NSPR may still be
appropriate as the platform dependent layer under Java, but its primary
application is supporting clients written entirely in C or C++.

.. _How_It_Works:

How It Works
~~~~~~~~~~~~

NSPR's goal is to provide uniform service over a wide range of operating
system environments. It strives to not export the *lowest common
denominator*, but to exploit the best features of each operating system
on which it runs, and still provide a uniform service across a wide
range of host offerings.

Threads
^^^^^^^

Threads are the major feature of NSPR. The industry's offering of
threads is quite sundry. NSPR, while far from perfect, does provide a
single API to which clients may program and expect reasonably consistent
behavior. The operating systems provide everything from no concept of
threading at all up to and including sophisticated, scalable and
efficient implementations. NSPR makes as much use of what the systems
offer as it can. It is a goal of NSPR that NSPR impose as little
overhead as possible in accessing those appropriate system features.

.. _Thread_synchronization:

Thread synchronization
^^^^^^^^^^^^^^^^^^^^^^

Thread synchronization is loosely based on Monitors as described by
C.A.R. Hoare in *Monitors: An operating system structuring concept* ,
Communications of the ACM, 17(10), October 1974 and then formalized by
Xerox' Mesa programming language ("Mesa Language Manual", J.G. Mitchell
et al, Xerox PARC, CSL-79-3 (Apr 1979)). This mechanism provides the
basic mutual exclusion (mutex) and thread notification facilities
(condition variables) implemented by NSPR. Additionally, NSPR provides
synchronization methods more suited for use by Java. The Java-like
facilities include monitor *reentrancy*, implicit and tightly bound
notification capabilities with the ability to associate the
synchronization objects dynamically.

.. _I.2FO:

I/O
^^^

NSPR's I/O is a slightly augmented BSD sockets model that allows
arbitrary layering. It was originally intended to export synchronous I/O
methods only, relying on threads to provide the concurrency needed for
complex applications. That method of operation is preferred though it is
possible to configure the network I/O channels as *non-blocking* in the
traditional sense.

.. _Network_addresses:

Network addresses
^^^^^^^^^^^^^^^^^

Part of NSPR deals with manipulation of network addresses. NSPR defines
a network address object that is Internet Protocol (IP) centric. While
the object is not declared as opaque, the API provides methods that
allow and encourage clients to treat the addresses as polymorphic items.
The goal in this area is to provide a migration path between IPv4 and
IPv6. To that end it is possible to perform translations of ASCII
strings (DNS names) into NSPR's network address structures, with no
regard to whether the addressing technology is IPv4 or IPv6.

Time
^^^^

Timing facilities are available in two forms: interval timing and
calendar functions.

Interval timers are based on a free running, 32-bit, platform dependent
resolution timer. Such timers are normally used to specify timeouts on
I/O, waiting on condition variables and other rudimentary thread
scheduling. Since these timers have finite namespace and are free
running, they can wrap at any time. NSPR does not provide an *epoch* ,
but expects clients to deal with that issue. The *granularity* of the
timers is guaranteed to be between 10 microseconds and 1 millisecond.
This allows a minimal timer *period* in of approximately 12 hours. But
in order to deal with the wrap-around issue, only half that namespace
may be utilized. Therefore, the minimal usable interval available from
the timers is slightly less than six hours.

Calendar times are 64-bit signed numbers with units of microseconds. The
*epoch* for calendar times is midnight, January 1, 1970, Greenwich Mean
Time. Negative times extend to times before 1970, and positive numbers
forward. Use of 64 bits allows a representation of times approximately
in the range of -30000 to the year 30000. There is a structural
representation (*i.e., exploded* view), routines to acquire the current
time from the host system, and convert them to and from the 64-bit and
structural representation. Additionally there are routines to convert to
and from most well-known forms of ASCII into the 64-bit NSPR
representation.

.. _Memory_management:

Memory management
^^^^^^^^^^^^^^^^^

NSPR provides API to perform the basic malloc, calloc, realloc and free
functions. Depending on the platform, the functions may be implemented
almost entirely in the NSPR runtime or simply shims that call
immediately into the host operating system's offerings.

Linking
^^^^^^^

Support for linking (shared library loading and unloading) is part of
NSPR's feature set. In most cases this is simply a smoothing over of the
facilities offered by the various platform providers.

Where It's Headed
~~~~~~~~~~~~~~~~~

NSPR is applicable as a platform on which to write threaded applications
that need to be ported to multiple platforms.

NSPR is functionally complete and has entered a mode of sustaining
engineering. As operating system vendors issue new releases of their
operating systems, NSPR will be moved forward to these new releases by
interested players.
