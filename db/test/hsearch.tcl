# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)hsearch.tcl	10.2 (Sleepycat) 4/10/98
#
# Historic Hsearch interface test.
# Use the first 1000 entries from the dictionary.
# Insert each with self as key and data; retrieve each.
# After all are entered, retrieve all; compare output to original.
# Then reopen the file, re-retrieve everything.
# Finally, delete everything.
proc htest { { nentries 1000 } } {
	puts "HSEARCH interfaces test: $nentries"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir

	error_check_good hcreate [hcreate $nentries] 0
	set did [open $dict]
	set count 0

	puts "\tHSEARCH.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	while { [gets $did str] != -1 && $count < $nentries } {
		set ret [hsearch $str $str enter]
		error_check_good hsearch:enter $ret 0

		set d [hsearch $str 0 find]
		error_check_good hsearch:find $d $str
		incr count
	}
	close $did

	puts "\tHSEARCH.b: re-get loop"
	set did [open $dict]
	# Here is the loop where we retrieve each key
	while { [gets $did str] != -1 && $count < $nentries } {
		set d [hsearch $str 0 find]
		error_check_good hsearch:find $d $str
		incr count
	}
	close $did
	error_check_good hdestroy [hdestroy] 0
}
