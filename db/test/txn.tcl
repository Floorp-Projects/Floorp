# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)txn.tcl	10.9 (Sleepycat) 4/22/98
#
# Options are:
# -dir <directory in which to store memp>
# -max <max number of concurrent transactions>
# -iterations <iterations>
# -stat
proc txn_usage {} {
	puts "txn -dir <directory> -iterations <number of ops> \
	    -max <max number of transactions> -stat"
}
proc txntest { args } {
source ./include.tcl

# Set defaults
	set iterations 50
	set max 1024
	set dostat 0
	set flags 0
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-d.* { incr i; set testdir [lindex $args $i] }
			-f.* { incr i; set flags [lindex $args $i] }
			-i.* { incr i; set iterations [lindex $args $i] }
			-m.* { incr i; set max [lindex $args $i] }
			-s.* { set dostat 1 }
			default {
				txn_usage
				return
			}
		}
	}
	if { [file exists $testdir] != 1 } {
		exec $MKDIR $testdir
	} elseif { [file isdirectory $testdir ] != 1 } {
		error "$testdir is not a directory"
	}

	# Clean out old txn if it existed
	puts "Unlinking txn: error message OK"
	txn_unlink $testdir 1

	# Now run the various functionality tests
	txn001 $testdir $max $flags
	txn002 $testdir $max $iterations $flags
	txn003 $testdir $flags
	txn004 $testdir $iterations
}

proc txn001 { dir max flags } {
source ./include.tcl
puts "Txn001: Open/Close/Create/Unlink test"
	# Try opening without Create flag should error
	set tp [ txn "" 0 0 ]
	error_check_good txn:fail $tp NULL

	# Now try opening with create
	set tp [txn "" [expr $DB_CREATE | $flags] 0644 -maxtxns $max ]
	error_check_bad txn:$dir $tp NULL
	error_check_good txn:$dir [is_valid_widget $tp mgr] TRUE

	# Make sure that close works.
	error_check_good txn_close:$tp [$tp close] 0

	# Make sure we can reopen.
	set tp [ txn "" $flags 0 ]
	error_check_bad txn:$dir $tp NULL
	error_check_good txn:$dir [is_substr $tp mgr] 1

	# Try unlinking while we're still attached, should fail.
	error_check_good txn_unlink:$dir [txn_unlink $testdir 0] -1

	# Now close it and unlink it
	error_check_good txn_close:$tp [$tp close] 0
	error_check_good txn_unlink:$dir [txn_unlink $testdir 0] 0
}

proc txn002 { dir max ntxns flags} {
source ./include.tcl
	puts "Txn002: Basic begin, commit, abort"

	set e [dbenv -dbflags [expr $DB_CREATE | $DB_INIT_TXN | $flags]]
	error_check_good dbenv [is_valid_widget $e env] TRUE
	set tp [ txn "" 0 0644 -maxtxns $max -dbenv $e ]

	error_check_bad txn:$dir $tp NULL
	error_check_good txn:$dir [is_substr $tp mgr] 1

	# We will create a bunch of transactions and commit them.
	set txn_list {}
	set tid_list {}
	puts "Txn002.a: Beginning/Committing Transactions"
	for { set i 0 } { $i < $ntxns } { incr i } {
		set txn [$tp begin]
		error_check_good txn_begin [is_substr $txn $tp] 1
		error_check_bad txn_begin $txn NULL
		lappend txn_list $txn
		set tid [$txn id]
		error_check_good tid_check [lsearch $tid_list $tid] -1
		lappend tid_list $tid
	}

	# Now commit them all
	foreach t $txn_list {
		error_check_good txn_commit:$t [$t commit] 0
	}

	# We will create a bunch of transactions and abort them.
	set txn_list {}
	puts "Txn002.b: Beginning/Aborting Transactions"
	for { set i 0 } { $i < $ntxns } { incr i } {
		set txn [$tp begin]
		error_check_good txn_begin [is_substr $txn $tp] 1
		error_check_bad txn_begin $txn NULL
		lappend txn_list $txn
		set tid [$txn id]
		error_check_good tid_check [lsearch $tid_list $tid] -1
		lappend tid_list $tid
	}

	# Now abort them all
	foreach t $txn_list {
		error_check_good txn_abort:$t [$t abort] 0
	}

	# We will create a bunch of transactions and commit them.
	set txn_list {}
	puts "Txn002.c: Beginning/Prepare/Committing Transactions"
	for { set i 0 } { $i < $ntxns } { incr i } {
		set txn [$tp begin]
		error_check_good txn_begin [is_substr $txn $tp] 1
		error_check_bad txn_begin $txn NULL
		lappend txn_list $txn
		set tid [$txn id]
		error_check_good tid_check [lsearch $tid_list $tid] -1
		lappend tid_list $tid
	}

	# Now prepare them all
	foreach t $txn_list {
		error_check_good txn_prepare:$t [$t prepare] 0
	}

	# Now commit them all
	foreach t $txn_list {
		error_check_good txn_commit:$t [$t commit] 0
	}

	# Close and unlink the file
	if { $e == "NULL" } {
		error_check_good txn_close:$tp [$tp close] 0
	} else {
		rename $e ""
	}
	error_check_good txn_unlink:$dir [txn_unlink $testdir 0] 0
}

proc txn003 { dir flags } {
source ./include.tcl
	puts "Txn003: Transaction grow region test"

	set tp [ txn "" [expr $DB_CREATE | $DB_THREAD] 0644 -maxtxns 10 ]
	error_check_bad txn:$dir $tp NULL
	error_check_good txn:$dir [is_substr $tp mgr] 1

	# Create initial 10 transactions.
	set txn_list {}
	puts "Txn003.a: Creating initial set of transactions"
	for { set i 0 } { $i < 10 } { incr i } {
		set txn [$tp begin]
		error_check_good txn_begin [is_substr $txn $tp] 1
		error_check_bad txn_begin $txn NULL
		lappend txn_list $txn
	}

	# Create next set of transactions to grow region
	puts "Txn003.b: Creating transactions to grow region"
	for { set i 0 } { $i < 100 } { incr i } {
		set txn [$tp begin]
		error_check_good txn_begin [is_substr $txn $tp] 1
		error_check_bad txn_begin $txn NULL
		lappend txn_list $txn
	}

	# Now, randomly commit/abort the transactions
	foreach t $txn_list {
		if { [random_int 1 2] == 1 } {
			error_check_good txn_commit:$t [$t commit] 0
		} else {
			error_check_good txn_abort:$t [$t abort] 0
		}
	}

	# Close and unlink the file
	error_check_good txn_close:$tp [$tp close] 0
	error_check_good txn_unlink:$dir [txn_unlink $testdir 0] 0
}

# Verify that read-only transactions do not create
# any log records
proc txn004 { dir ntxns } {
	puts "Txn004: Read-only transaction test"
	source ./include.tcl

	cleanup $dir
	set e [dbenv -dbflags [expr $DB_CREATE | $DB_INIT_TXN | $DB_INIT_LOG]]
	error_check_good dbenv [is_valid_widget $e env] TRUE
	set tp [ txn "" 0 0644 -dbenv $e ]
	error_check_bad txn:$dir $tp NULL
	error_check_good txn:$dir [is_substr $tp mgr] 1

	set log [ log "" 0 0644 -dbenv $e ]
	error_check_bad log:$dir $log NULL
	error_check_good log:$dir:$log [is_substr $log log] 1

	# We will create a bunch of transactions and commit them.
	set txn_list {}
	set tid_list {}
	puts "Txn004.a: Beginning/Committing Transactions"
	for { set i 0 } { $i < $ntxns } { incr i } {
		set txn [$tp begin]
		error_check_good txn_begin [is_substr $txn $tp] 1
		error_check_bad txn_begin $txn NULL
		lappend txn_list $txn
		set tid [$txn id]
		error_check_good tid_check [lsearch $tid_list $tid] -1
		lappend tid_list $tid
	}

	# Now commit them all
	foreach t $txn_list {
		error_check_good txn_commit:$t [$t commit] 0
	}

	# Now verify that there aren't any log records.
	set r [$log get {0 0} $DB_FIRST ]
	error_check_good log_get:$r [llength $r] 0

	error_check_good log_close [$log close] 0
	error_check_good txn_close [$tp close] 0
	reset_env $e
}
