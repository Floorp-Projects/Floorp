# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)recd004.tcl	8.2 (Sleepycat) 4/10/98
#
# Recovery Test #2.  Verify that we work correctly when big keys
# get elevated.
proc recd004 { method {select 0} } {
	set opts [convert_args $method ""]
	set method [convert_method $method]
	if { $method == "DB_RECNO" } {
		puts "Recd003 skipping for method $method"
		return
	}
	puts "Recd004: $method big-key on internal page recovery tests"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	cleanup $testdir
	set testfile recd004.db
	set flags [expr $DB_CREATE | $DB_THREAD | \
	    $DB_INIT_LOG | $DB_INIT_LOCK | $DB_INIT_MPOOL | $DB_INIT_TXN]

	puts "\tRecd004.a: creating environment"
	set env_cmd "dbenv -dbhome $testdir -dbflags $flags"
	set dbenv [eval $env_cmd]
	error_check_bad dbenv $dbenv NULL

	# Create the database. We will use a small page size so that we
	# elevate quickly
	set db [dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE | $DB_THREAD] \
	    0644 $method -psize 512 -dbenv $dbenv $opts]
	error_check_bad db_open $db NULL
	error_check_good db_open [is_substr $db db] 1
	error_check_good db_close [$db close] 0
	reset_env $dbenv

	# List of recovery tests: {CMD MSG} pairs
	set slist {
		{ {big_populate DB TXNID $n} "Recd004.b: big key elevation"}
		{ {unpopulate DB TXNID 0} "Recd004.c: Remove keys"}
	}

	# If pages are 512 bytes, then adding 512 key/data pairs
	# should be more than sufficient.
	set n 512
	foreach pair $slist {
		set cmd [my_subst [lindex $pair 0]]
		set msg [lindex $pair 1]
		if { $select != 0 } {
			set tag [lindex $msg 0]
			set tail [expr [string length $tag] - 2]
			set tag [string range $tag $tail $tail]
			if { [lsearch $select $tag] == -1 } {
				continue
			}
		}
		op_recover abort $testdir $env_cmd $testfile $cmd $msg
		op_recover commit $testdir $env_cmd $testfile $cmd $msg
	}
}
