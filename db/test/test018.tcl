# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test018.tcl	10.3 (Sleepycat) 4/26/98
#
# DB Test 18 {access method}
# Run duplicates with small page size so that we test off page duplicates.
proc test018 { method {nentries 10000} args} {
	puts "Test018: Off page duplicate tests"
	set cmd [concat "test011 $method $nentries 19 18 -psize 512" $args]
	eval $cmd
}
