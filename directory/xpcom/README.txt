This is the home for XPCOM modules that implement LDAP functionality.

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

datasource/
	An RDF datasource, written in Javascript. 

tests/
	Some basic tests to help ensure that things don't break as
        development proceeds.

Building
--------
See <http://www.mozilla.org/directory/xpcom.html>.

Dan Mosedale <dmose@mozilla.org>
