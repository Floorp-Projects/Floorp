This is the home for XPCOM modules that implement LDAP functionality.

WARNING: This code is NOT YET READY FOR PRIME-TIME.  You ALMOST
CERTAINLY DON'T WANT TO BUILD IT unless you're planning to hack on the
code.  It is currently synchronous, fragile, and only builds on unix.
It is checked in to CVS primarily to allow folks to lend a hand.

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
