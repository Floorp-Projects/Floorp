# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)byteorder.tcl	8.4 (Sleepycat) 4/10/98
#
# Byte Order Test
# Use existing tests and run with both byte orders.
proc byteorder { method {nentries 1000} } {
	set opts [convert_args $method ""]
	set method [convert_method $method]
	puts "Byteorder: $method $nentries"

	test001 $method $nentries -order 1234 $opts
	test001 $method $nentries -order 4321 $opts
	test003 $method -order 1234 $opts
	test003 $method -order 4321 $opts
	test010 $method $nentries 5 10 -order 1234 $opts
	test010 $method $nentries 5 10 -order 4321 $opts
	test011 $method $nentries 5 11 -order 1234 $opts
	test011 $method $nentries 5 11 -order 4321 $opts
	test018 $method $nentries -order 1234 $opts
	test018 $method $nentries -order 4321 $opts
}
