This is the home for XPCOM modules that implement LDAP functionality.

What's Here
-----------
base/	
	Implements a wrapper around the LDAP C SDK, as well as support
	for ldap: URLs in the browser.  Written entirely in C++;
	theoretically only depends on necko, xpcom, nspr, and the LDAP
	C SDK.

datasource/
	An RDF datasource, written in Javascript. 

tests/
	Some basic tests to help ensure that things don't break as
        development proceeds.  Currently, there is only some stuff for
	testing the datasource.


Building
--------
See <http://www.mozilla.org/directory/xpcom.html>.

Dan Mosedale <dmose@mozilla.org>
