# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)recd005.tcl	8.5 (Sleepycat) 5/3/98
#
# Recovery Test 5.
# Make sure that we can do catastrophic recovery even if we open
# files using the same log file id.
proc recd005 { method {select 0} } {
	set method [convert_method $method]
	puts "Recd005: $method catastropic recovery"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	set testfile1 recd005.1.db
	set testfile2 recd005.2.db
	set flags [expr $DB_CREATE | $DB_THREAD | \
	    $DB_INIT_LOG | $DB_INIT_LOCK | $DB_INIT_MPOOL | $DB_INIT_TXN]

	set tnum 0
	foreach sizes "{1000 10} {10 1000}" {
		foreach ops "{abort abort} {abort commit} {commit abort} \
		{commit commit}" {
			cleanup $testdir
			incr tnum

			set s1 [lindex $sizes 0]
			set s2 [lindex $sizes 1]
			set op1 [lindex $ops 0]
			set op2 [lindex $ops 1]
			puts "\tRecd005.$tnum: $s1 $s2 $op1 $op2"

			puts "\tRecd005.$tnum.a: creating environment"
			set env_cmd "dbenv -dbhome $testdir -dbflags $flags"
			set dbenv [eval $env_cmd]
			error_check_bad dbenv $dbenv NULL

			# Create the two databases.
			set db1 [dbopen $testfile1 \
			    [expr $DB_CREATE | $DB_TRUNCATE | $DB_THREAD] \
			    0644 $method -dbenv $dbenv]
			error_check_bad db_open $db1 NULL
			error_check_good db_open [is_substr $db1 db] 1
			error_check_good db_close [$db1 close] 0

			set db2 [dbopen $testfile2 \
			    [expr $DB_CREATE | $DB_TRUNCATE | $DB_THREAD] \
			    0644 $method -dbenv $dbenv]
			error_check_bad db_open $db2 NULL
			error_check_good db_open [is_substr $db2 db] 1
			error_check_good db_close [$db2 close] 0
			reset_env $dbenv

			set env [eval $env_cmd]
			puts "\tRecd005.$tnum.b: Populating databases"
			do_one_file $testdir $method $env $testfile1 $s1 $op1
			do_one_file $testdir $method $env $testfile2 $s2 $op2

			puts "\tRecd005.$tnum.c: Verifying initial population"
			check_file $testdir $env $testfile1 $op1
			check_file $testdir $env $testfile2 $op2

			# Now, close the environment (so that recovery will work
			# on NT which won't allow delete of an open file).
			reset_env $env

			debug_check
			puts -nonewline "\tRecd005.$tnum.d: About to run recovery ... "
			flush stdout

			set stat \
			    [catch {exec ./db_recover -h $testdir -c} result]
			if { $stat == 1 && [is_substr $result \
			    "db_recover: Recovering the log"] == 0 } {
				error "Recovery error: $result."
			}
			puts "complete"


			# Substitute a file that will need recovery and try
			# running recovery again.
			if { $op1 == "abort" } {
				catch { exec $CP $dir/$testfile1.afterop \
				    $dir/$testfile1 } res
			} else {
				catch { exec $CP $dir/$testfile1.init \
				    $dir/$testfile1 } res
			}
			if { $op2 == "abort" } {
				catch { exec $CP $dir/$testfile2.afterop \
				    $dir/$testfile2 } res
			} else {
				catch { exec $CP $dir/$testfile2.init \
				    $dir/$testfile2 } res
			}
			debug_check
			puts -nonewline \
			    "\tRecd005.$tnum.e: About to run recovery on pre-op database ... "
			flush stdout

			set stat \
			    [catch {exec ./db_recover -h $testdir -c} result]
			if { $stat == 1 &&
			    [is_substr $result \
				"db_recover: Recovering the log"] == 0 } {
				error "Recovery error: $result."
			}
			puts "complete"

			set env [eval $env_cmd]
			check_file $testdir $env $testfile1 $op1
			check_file $testdir $env $testfile2 $op2
			reset_env $env
		}
	}
}

proc do_one_file { dir method env filename num op } {
	source ./include.tcl
	set init_file $dir/$filename.t1
	set afterop_file $dir/$filename.t2
	set final_file $dir/$filename.t3

	# Save the initial file and open the environment and the first file
	catch { exec $CP $dir/$filename $dir/$filename.init } res
	set nolock_env [$env simpledup]
	set tmgr [txn "" 0 0 -dbenv $env]
	set db [dbopen $filename 0 0 DB_UNKNOWN -dbenv $env]

	# Dump out file contents for initial case
	open_and_dump_file $filename $env 0 $init_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT

	set txn [$tmgr begin]
	error_check_bad txn_begin $txn NULL
	error_check_good txn_begin [is_substr $txn $tmgr] 1

	# Now fill in the db and the txnid in the command
	populate $db $method $txn $num 0 0

	# Sync the file so that we can capture a snapshot to test
	# recovery.
	error_check_good sync:$db [$db sync 0] 0
	catch { exec $CP $dir/$filename $dir/$filename.afterop } res
	open_and_dump_file $filename.afterop $nolock_env 0 $afterop_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT
	error_check_good txn_$op:$txn [$txn $op] 0

	if { $op == "commit" } {
		puts "\t\tFile $filename executed and committed."
	} else {
		puts "\t\tFile $filename executed and aborted."
	}

	# Dump out file and save a copy.
	error_check_good sync:$db [$db sync 0] 0
	open_and_dump_file $filename $nolock_env 0 $final_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT
	catch { exec $CP $dir/$filename $dir/$filename.final } res

	# If this is an abort, it should match the original file.
	# If this was a commit, then this file should match the
	# afterop file.
	if { $op == "abort" } {
		exec $SORT $init_file > $init_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(initial,post-$op):diff($init_file,$final_file) \
		    [catch { exec $DIFF $init_file.sort $final_file.sort } res] 0
	} else {
		exec $SORT $afterop_file > $afterop_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(post-$op,pre-commit):diff($afterop_file,$final_file) \
		    [catch { exec $DIFF $afterop_file.sort $final_file.sort } res] 0
	}

	error_check_good close:$db [$db close] 0
	error_check_good txn_close [$tmgr close] 0
}

proc check_file { dir env filename op } {
	source ./include.tcl

	set init_file $dir/$filename.t1
	set afterop_file $dir/$filename.t2
	set final_file $dir/$filename.t3

	set nolock_env [$env simpledup]
	open_and_dump_file $filename $nolock_env 0 $final_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT
	if { $op == "abort" } {
		exec $SORT $init_file > $init_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(initial,post-$op):diff($init_file,$final_file) \
		    [catch { exec $DIFF $init_file.sort $final_file.sort } res] 0
	} else {
		exec $SORT $afterop_file > $afterop_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(pre-commit,post-$op):diff($afterop_file,$final_file) \
		    [catch { exec $DIFF $afterop_file.sort $final_file.sort } res] 0
	}
}
