# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)recd003.tcl	10.6 (Sleepycat) 4/10/98
#
# Recovery Test 3.
# Test all the duplicate log messages and recovery operations.  We make
# sure that we exercise all possible recovery actions: redo, undo, undo
# but no fix necessary and redo but no fix necessary.
proc recd003 { method {select 0} } {
	set method [convert_method $method]
	if { $method == "DB_RECNO" } {
		puts "Recd003 skipping for method RECNO"
		return
	}
	puts "Recd003: $method duplicate recovery tests"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	cleanup $testdir
	set testfile recd003.db
	set flags [expr $DB_CREATE | $DB_THREAD | \
	    $DB_INIT_LOG | $DB_INIT_LOCK | $DB_INIT_MPOOL | $DB_INIT_TXN]

	puts "\tRecd003.a: creating environment"
	set env_cmd "dbenv -dbhome $testdir -dbflags $flags"
	set dbenv [eval $env_cmd]
	error_check_bad dbenv $dbenv NULL

	# Create the database.
	set db [dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE | $DB_THREAD] \
	    0644 $method -flags $DB_DUP -dbenv $dbenv]
	error_check_bad db_open $db NULL
	error_check_good db_open [is_substr $db db] 1
	error_check_good db_close [$db close] 0
	reset_env $dbenv


	# List of recovery tests: {CMD MSG} pairs
	set dlist {
	{ {populate DB $m TXNID $n 1 0}	 "Recd003.b: add dups"}
	{ {DB del TXNID duplicate_key 0} "Recd003.c: remove dups all at once"}
	{ {populate DB $m TXNID $n 1 0}	 "Recd003.d: add dups (change state)"}
	{ {unpopulate DB TXNID 0}	 "Recd003.e: remove dups 1 at a time"}
	{ {populate DB $m TXNID $dupn 1 0} "Recd003.f: dup split"}
	{ {DB del TXNID duplicate_key 0}
	  "Recd003.g: remove dups (change state)"}
	{ {populate DB $m TXNID $n 1 1}	 "Recd003.h: add big dup"}
	{ {DB del TXNID duplicate_key 0}
	  "Recd003.i: remove big dup all at once"}
	{ {populate DB $m TXNID $n 1 1}
	  "Recd003.j: add big dup (change state)"}
	{ {unpopulate DB TXNID 0}       "Recd003.k: remove big dup 1 at a time"}
	{ {populate DB $m TXNID $bign 1 1} "Recd003.l: split big dup"}
	}

	# These are all the data values that we're going to need to read
	# through the operation table and run the recovery tests.
	set m $method
	set n 10
	set dupn 2000
	set bign 500

	foreach pair $dlist {
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
