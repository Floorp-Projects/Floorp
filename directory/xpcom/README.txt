This is the home for XPCOM modules that implement LDAP functionality.

WARNING: This code is not quite ready for prime time, and has only
been built on Linux.  You probably don't want to build it yet unless
you're planning to hack on the code.  Once asynchronicity and a bunch
of build glue gets added, then perhaps it will be reasonable to add
this code to the general SeaMonkeyAll checkout and builds.  Hopefully
these chores won't take too long to do (I've already started work on
making the code async); the first four or five items in TODO.txt are
the important ones.  I'm checking it into CVS at this stage primarily
to allow fellow hackers to lend a hand.

What's Here
-----------
Right now, there is only the base component:

base/	
	Implements a wrapper around the LDAP C SDK, as well as support
	for ldap: URLs in the browser.  Written entirely in C++;
	theoretically only depends on necko, xpcom, nspr, and the LDAP
	C SDK.

In the not-too-distant future, other components are likely to appear,
including:

rdf/
	An RDF datasource, probably written in Javascript. 

tests/
	Some basic tests to help ensure that things don't break as
        development proceeds.

Building
--------
This requires various other pieces of infrastructure from the Mozilla
CVS tree.  See <http://www.mozilla.org/directory/xpcom.html>
for now.

Dan Mosedale <dmose@mozilla.org>
