# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)recd002.tcl	10.6 (Sleepycat) 4/10/98
#
# Recovery Test #2.  Verify that splits can be recovered.
proc recd002 { method {select 0} } {
	set opts [convert_args $method ""]
	set method [convert_method $method]
	puts "Recd002: $method split recovery tests"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	cleanup $testdir
	set testfile recd002.db
	set flags [expr $DB_CREATE | $DB_THREAD | \
	    $DB_INIT_LOG | $DB_INIT_LOCK | $DB_INIT_MPOOL | $DB_INIT_TXN]

	puts "\tRecd002.a: creating environment"
	set env_cmd "dbenv -dbhome $testdir -dbflags $flags"
	set dbenv [eval $env_cmd]
	error_check_bad dbenv $dbenv NULL

	# Create the database. We will use a small page size so that splits
	# happen fairly quickly.
	set db [dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE | $DB_THREAD] \
	    0644 $method -psize 512 -dbenv $dbenv $opts]
	error_check_bad db_open $db NULL
	error_check_good db_open [is_substr $db db] 1
	error_check_good db_close [$db close] 0
	reset_env $dbenv

	# List of recovery tests: {CMD MSG} pairs
	set slist {
		{ {populate DB $method TXNID $n 0 0} "Recd002.b: splits"}
		{ {unpopulate DB TXNID $r} "Recd002.c: Remove keys"}
	}

	# If pages are 512 bytes, then adding 512 key/data pairs
	# should be more than sufficient.
	set n 512
	set r [expr $n / 2 ]
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
