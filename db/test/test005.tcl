# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test005.tcl	10.2 (Sleepycat) 4/10/98
#
# DB Test 5 {access method}
# Check that cursor operations work.  Create a database; close database and
# reopen it.  Then read through the database sequentially using cursors and
# delete each element.
proc test005 { method {nentries 10000} args } {
	test004 $method $nentries 5 0 $args
}
