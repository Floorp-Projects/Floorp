# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)recd001.tcl	10.8 (Sleepycat) 4/10/98
#
# Recovery Test 1.
# These are the most basic recovery tests.  We do individual recovery
# tests for each operation in the access method interface.  First we
# create a file and capture the state of the database (i.e., we copy
# it.  Then we run a transaction containing a single operation.  In
# one test, we abort the transaction and compare the outcome to the
# original copy of the file.  In the second test, we restore the
# original copy of the database and then run recovery and compare
# this against the actual database.
proc recd001 { method {select 0} } {
	set opts [convert_args $method ""]
	set method [convert_method $method]
	puts "Recd001: $method operation/transaction tests"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and environment.
	cleanup $testdir

	set testfile recd001.db
	set flags [expr $DB_CREATE | $DB_THREAD | \
	    $DB_INIT_LOG | $DB_INIT_LOCK | $DB_INIT_MPOOL | $DB_INIT_TXN]

	puts "\tRecd001.a: creating environment"
	set env_cmd "dbenv -dbhome $testdir -dbflags $flags"
	set dbenv [eval $env_cmd]
	error_check_bad dbenv $dbenv NULL

	# Create the database and close the environment.
	set db [dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE | $DB_THREAD] \
	    0644 $method -dbenv $dbenv $opts]
	error_check_bad db_open $db NULL
	error_check_good db_open [is_substr $db db] 1
	error_check_good db_close [$db close] 0
	reset_env $dbenv

	# List of recovery tests: {CMD MSG} pairs
	set rlist {
	{ {DB put TXNID $key $data 0}	"Recd001.b: put"}
	{ {DB del TXNID $key 0}		"Recd001.c: delete"}
	{ {DB put TXNID $bigkey $data 0} "Recd001.d: big key put"}
	{ {DB del TXNID $bigkey 0}	"Recd001.e: big key delete"}
	{ {DB put TXNID $key $bigdata 0} "Recd001.f: big data put"}
	{ {DB del TXNID $key 0}		"Recd001.g: big data delete"}
	{ {DB put TXNID $key $data 0}	"Recd001.h: put (change state)"}
	{ {DB put TXNID $key $newdata 0} "Recd001.i: overwrite"}
	{ {DB put TXNID $key $partial_grow $DB_DBT_PARTIAL $off $len}
	  "Recd001.j: partial put growing"}
	{ {DB put TXNID $key $newdata 0} "Recd001.k: overwrite (fix)"}
	{ {DB put TXNID $key $partial_shrink $DB_DBT_PARTIAL $off $len}
	  "Recd001.l: partial put growing"}
	}

	# These are all the data values that we're going to need to read
	# through the operation table and run the recovery tests.

	if { $method == "DB_RECNO" } {
		set key 1
	} else {
		set key recd001_key
	}
	set data recd001_data
	set newdata NEWrecd001_dataNEW
	set off 3
	set len 12
	set partial_grow replacement_record_grow
	set partial_shrink xxx
	set bigdata [replicate $key 1000]
	if { $method == "DB_RECNO" } {
		set bigkey 1000
	} else {
		set bigkey [replicate $key 1000]
	}

	foreach pair $rlist {
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
