# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test022.tcl	8.4 (Sleepycat) 4/10/98
#
# DB Test 22 {access method}
# Test multiple data directories.  Do a bunch of different opens
# to make sure that the files are detected in different directories.
proc test022 { method args } {
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test022: $method ($args) multiple data directory test"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl
	cleanup $testdir
	exec $MKDIR $testdir/data1
	exec $MKDIR $testdir/data2
	exec $MKDIR $testdir/data3

	puts "Test022.a: Multiple data directories in DB_CONFIG file"

	# Create a config file
	set cid [open $testdir/DB_CONFIG w]
	puts $cid "DB_DATA_DIR data1"
	puts $cid "DB_DATA_DIR data2"
	puts $cid "DB_DATA_DIR data3"
	close $cid

	# Now get pathnames
	set curdir [pwd]
	cd $testdir
	set fulldir [pwd]
	cd $curdir

	set e [dbenv]
	ddir_test $fulldir $method $e $args
	rename $e ""

	puts "Test022.b: Multiple data directories in db_appinit call."
	cleanup $testdir
	exec $MKDIR $testdir/data1
	exec $MKDIR $testdir/data2
	exec $MKDIR $testdir/data3

	# Now call dbenv with config specified
	set e [dbenv -dbconfig \
	    { {DB_DATA_DIR data1} {DB_DATA_DIR data2} {DB_DATA_DIR data3}} ]
	ddir_test $fulldir $method $e $args
	rename $e ""

	cleanup $testdir
}

proc ddir_test { fulldir m e args } {
	source ./include.tcl

	# Now create one file in each directory
	set db1 [eval [concat dbopen $fulldir/data1/datafile1.db \
	    [expr $DB_CREATE | $DB_TRUNCATE] 0644 $m -dbenv $e $args]]
	error_check_good dbopen1 [is_valid_db $db1] TRUE

	set db2 [eval [concat dbopen $fulldir/data2/datafile2.db \
	    [expr $DB_CREATE | $DB_TRUNCATE] 0644 $m -dbenv $e $args]]
	error_check_good dbopen2 [is_valid_db $db2] TRUE

	set db3 [eval [concat dbopen $fulldir/data3/datafile3.db \
	    [expr $DB_CREATE | $DB_TRUNCATE] 0644 $m -dbenv $e $args]]
	error_check_good dbopen3 [is_valid_db $db3] TRUE

	# Close the files
	error_check_good db_close1 [$db1 close] 0
	error_check_good db_close2 [$db2 close] 0
	error_check_good db_close3 [$db3 close] 0

	# Now, reopen the files without complete pathnames and make
	# sure that we find them.

	set db1 [dbopen datafile1.db 0 0 DB_UNKNOWN -dbenv $e]
	error_check_good dbopen1 [is_valid_db $db1] TRUE

	set db2 [dbopen datafile2.db 0 0 DB_UNKNOWN -dbenv $e]
	error_check_good dbopen2 [is_valid_db $db2] TRUE

	set db3 [dbopen datafile3.db 0 0 DB_UNKNOWN -dbenv $e]
	error_check_good dbopen3 [is_valid_db $db3] TRUE

	# Finally close all the files
	error_check_good db_close1 [$db1 close] 0
	error_check_good db_close2 [$db2 close] 0
	error_check_good db_close3 [$db3 close] 0
}
