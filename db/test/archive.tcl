# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)archive.tcl	10.8 (Sleepycat) 6/2/98
#
# Options are:
# -checkrec <checkpoint frequency"
# -dir <dbhome directory>
# -maxfilesize <maxsize of log file>
# -stat
proc archive_usage {} {
	puts "archive -checkrec <checkpt freq> -dir <directory> \
	    -maxfilesize <max size of log files>"
}
proc archive { args } {
	source ./include.tcl

# Set defaults
global alphabet
	set maxfile [expr 1024 * 20]
	set dostat 0
	set checkrec 500
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-c.* { incr i; set checkrec [lindex $args $i] }
			-d.* { incr i; set testdir [lindex $args $i] }
			-m.* { incr i; set maxfile [lindex $args $i] }
			-s.* { set dostat 1 }
			default {
				archive_usage
				return
			}

		}
	}
	if { [file exists $testdir] != 1 } {
		exec $MKDIR $testdir
	} elseif { [file isdirectory $testdir ] != 1 } {
		error "$testdir is not a directory"
	}

	# Clean out old log if it existed
	puts "Unlinking log: error message OK"
	cleanup $testdir

	# Now run the various functionality tests
	set dbenv [dbenv -dbhome $testdir -maxsize $maxfile -dbflags \
	    [expr $DB_CREATE | $DB_INIT_MPOOL | $DB_INIT_LOCK | \
	    $DB_INIT_LOG | $DB_INIT_TXN]]
	error_check_bad dbenv $dbenv NULL
	error_check_good dbenv [is_substr $dbenv env] 1

	# Open the log
	set lp [ log "" 0 0 -maxsize $maxfile -dbenv $dbenv]
	error_check_bad log:$testdir $lp NULL
	error_check_good log:$testdir [is_substr $lp log] 1

	# Open/Create the lock region
	set txn [txn "" 0 0 -dbenv $dbenv]
	error_check_bad txn $txn NULL
	error_check_good txn [is_substr $txn mgr] 1

	# The basic test structure here is that we write a lot of log
	# records (enough to fill up 100 log files; each log file it
	# small).  We take periodic checkpoints.  Between each pair
	# of checkpoints, we refer to 2 files, overlapping them each
	# checkpoint.  We also start transactions and let them overlap
	# checkpoints as well.  The pattern that we try to create is:
	# ---- write log records----|||||--- write log records ---
	# -T1 T2 T3 --- D1 D2 ------CHECK--- CT1 --- D2 D3 CD1 ----CHECK
	# where TX is begin transaction, CTx is commit transaction, DX is
	# open data file and CDx is close datafile.

	set baserec "1:$alphabet:2:$alphabet:3:$alphabet:4:$alphabet"
	puts "Archive.a: Writing log records; checkpoint every $checkrec records"
	set nrecs $maxfile
	set rec 0:$baserec
	# Begin transaction and write a log record
	set t1 [$txn begin]
	set l1 [lindex [$lp put $rec 0] 1]
	set lsnlist [list $l1]

	set t2 [$txn begin]
	set l1 [lindex [$lp put $rec 0] 1]
	lappend lsnlist $l1

	set t3 [$txn begin]
	set l1 [lindex [$lp put $rec 0] 1]
	lappend lsnlist $l1

	set txnlist [list $t1 $t2 $t3]
	set db1 [dbopen ar1 $DB_CREATE 0644 DB_HASH -dbenv $dbenv]
	set db2 [dbopen ar2 $DB_CREATE 0644 DB_BTREE -dbenv $dbenv]
	set dbcount 3
	set dblist [list $db1 $db2]
	for { set i 1 } { $i <= $nrecs } { incr i } {
		set rec $i:$baserec
		set lsn [$lp put $rec 0]
		error_check_bad log_put [is_substr $lsn log_cmd] 1

		if { [expr $i % $checkrec] == 0 } {
			# Take a checkpoint
			$txn check
			set ckp_file [lindex [$lp last] 0]

			catch { exec ./db_archive -h $testdir -a } res_log_full
			if { [string first db_archive $res_log_full] == 0 } {
				set res_log_full ""
			}
			catch { exec ./db_archive -h $testdir } res_log
			if { [string first db_archive $res_log] == 0 } {
				set res_log ""
			}
			catch { exec ./db_archive -h $testdir -l } res_alllog
			catch { exec ./db_archive -h $testdir -a -s } \
			    res_data_full
			catch { exec ./db_archive -h $testdir -s } res_data

			error_check_good nlogfiles [llength $res_alllog] \
			    [lindex [$lp last] 0]
			error_check_good logs_match [llength $res_log_full] \
			    [llength $res_log]
			error_check_good data_match [llength $res_data_full] \
			    [llength $res_data]

			# Check right number of log files
			error_check_good nlogs [llength $res_log] \
			    [expr [lindex $lsnlist 0] - 1]

			# Check that the relative names are a subset of the
			# full names
			set n 0
			foreach x $res_log {
				error_check_bad log_name_match:$res_log \
				    [string first $x \
				    [lindex $res_log_full $n]] -1
				incr n
			}

			set n 0
			foreach x $res_data {
				error_check_bad log_name_match:$res_data \
				    [string first $x \
				    [lindex $res_data_full $n]] -1
				incr n
			}

			# Begin/commit any transactions
			set t [lindex $txnlist 0]
			if { [string length $t] != 0 } {
				error_check_good txn_commit:$t [$t commit] 0
				set txnlist [lrange $txnlist 1 end]
			}
			set lsnlist [lrange $lsnlist 1 end]

			if { [llength $txnlist] == 0 } {
				set t1 [$txn begin]
				error_check_bad tx_begin $t1 NULL
				error_check_good tx_begin [is_substr $t1 $txn] 1
				set l1 [lindex [$lp put $rec 0] 1]
				lappend lsnlist [min $l1 $ckp_file]

				set t2 [$txn begin]
				error_check_bad tx_begin $t2 NULL
				error_check_good tx_begin [is_substr $t2 $txn] 1
				set l1 [lindex [$lp put $rec 0] 1]
				lappend lsnlist [min $l1 $ckp_file]

				set t3 [$txn begin]
				error_check_bad tx_begin $t3 NULL
				error_check_good tx_begin [is_substr $t3 $txn] 1
				set l1 [lindex [$lp put $rec 0] 1]
				lappend lsnlist [min $l1 $ckp_file]

				set txnlist [list $t1 $t2 $t3]
			}

			# Open/close some DB files
			if { [expr $dbcount % 2] == 0 } {
				set type DB_HASH
			} else {
				set type DB_BTREE
			}
			set db [dbopen ar$dbcount \
			    $DB_CREATE 0644 $type -dbenv $dbenv]
			error_check_bad db_open:$dbcount $db NULL
			error_check_good db_open:$dbcount [is_substr $db db] 1
			incr dbcount

			lappend dblist $db
			set db [lindex $dblist 0]
			error_check_good db_close:$db [$db close] 0
			set dblist [lrange $dblist 1 end]

		}
	}

	# Commit any transactions still running.
	foreach t $txnlist {
		error_check_good txn_commit:$t [$t commit] 0
	}

	# Close any files that are still open.
	foreach d $dblist {
		error_check_good db_close:$db [$d close] 0
	}

	# Close and unlink the file
	reset_env $dbenv
}

proc min { a b } {
	if {$a < $b} {
		return $a
	} else {
		return $b
	}
}
