# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test032.tcl	10.8 (Sleepycat) 4/26/98
#
# DB Test 32 {access method}
# System integration DB test: verify that locking, recovery, checkpoint,
# and all the other utilities basically work.
#
# The test consists of $nprocs processes operating on $nfiles files.  A
# transaction consists of adding the same key/data pair to some random
# number of these files.  We generate a bimodal distribution in key
# size with 70% of the keys being small (1-10 characters) and the
# remaining 30% of the keys being large (uniform distribution about
# mean $key_avg).  If we generate a key, we first check to make sure
# that the key is not already in the dataset.  If it is, we do a lookup.
#
# XXX This test uses grow-only files currently!

proc test032 { method {nprocs 5} {nfiles 10} {cont 0} args } {
source ./include.tcl

	set omethod $method
	if { $method != "all" } {
		set method [convert_method $method]
	}
	set args [convert_args $method $args]
	if { [is_rbtree $omethod] == 1 } {
		puts "Test032 skipping for method $omethod"
		return
	}

	puts "Test032: system integration test db $method $nprocs processes \
	    on $nfiles files"

	# Parse options
	set otherargs ""
	set seeds {}
	set key_avg 10
	set data_avg 20
	set do_exit 0
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-key_avg { incr i; set key_avg [lindex $args $i] }
			-data_avg { incr i; set data_avg [lindex $args $i] }
			-s.* { incr i; set seeds [lindex $args $i] }
			-testdir { incr i; set testdir [lindex $args $i] }
			-x.* { set do_exit 1 }
			default {
				lappend otherargs [lindex $args $i]
			}
		}
	}

	if { $cont == 0 } {

		if { [file exists $testdir] != 1 } {
			exec $MKDIR $testdir
		} elseif { [file isdirectory $testdir ] != 1 } {
			error "FAIL: $testdir is not a directory"
		}

		# Create the database and open the dictionary
		cleanup $testdir

		# Create an environment
		puts "\tTest032.a: creating environment and $nfiles files"
		set flags [expr $DB_INIT_MPOOL | $DB_INIT_LOCK \
		    | $DB_INIT_LOG | $DB_INIT_TXN]
		set dbenv  \
		    [eval dbenv -dbflags $flags -dbhome $testdir $otherargs]

		# Create a bunch of files
		set m $method
		for { set i 0 } { $i < $nfiles } { incr i } {
			if { $method == "all" } {
				switch [random_int 1 2] {
				1 { set m DB_BTREE }
				2 { set m DB_HASH }
				}
			}
			set otherargs [add_to_args $DB_DUP $otherargs]
			set db [eval [concat dbopen \
			    test032.$i.db $DB_CREATE 0644 $m \
			    -dbenv $dbenv $otherargs]]
			error_check_good dbopen [is_valid_db $db] TRUE
			error_check_good db_close [$db close] 0
		}
	}
	# Close the environment
	rename $dbenv {}

	if { $do_exit == 1 } {
		return
	}

	# Database is created, now fork off the kids.
	puts "\tTest032.b: forking off $nprocs processes and utilities"
	set cycle 1
	while { 1 } {
		# Fire off deadlock detector and checkpointer
		puts "Beginning cycle $cycle"
		set ddpid [exec ./db_deadlock -h $testdir -t 5 &]
		set cppid [exec ./db_checkpoint -h $testdir -p 2 &]
		puts "Deadlock detector: $ddpid Checkpoint daemon $cppid"

		set pidlist {}
		for { set i 0 } {$i < $nprocs} {incr i} {
			set s -1
			if { [llength $seeds] == $nprocs } {
				set s [lindex $seeds $i]
			}
			set p [exec ./dbtest ../test/sysscript.tcl $testdir \
			    $nfiles $key_avg $data_avg $s > \
			    $testdir/test032.$i.log & ]
			lappend pidlist $p
		}
		puts "[timestamp] $nprocs processes running $pidlist"
		exec $SLEEP [random_int 300 600]

		# Now simulate a crash
		puts "[timestamp] Crashing"
		exec $KILL -9 $ddpid
		exec $KILL -9 $cppid
		foreach p $pidlist {
			exec $KILL -9 $p
		}

		# Now run recovery
		test032_verify $testdir $nfiles
		incr cycle
	}
}

proc test032_usage { } {
	puts -nonewline "test032 method nentries [-d directory] [-i iterations]"
	puts " [-p procs] [-s {seeds} ] -x"
}

proc test032_verify { dir nfiles } {
source ./include.tcl
	# Save everything away in case something breaks
#	for { set f 0 } { $f < $nfiles } {incr f} {
#		exec $CP $dir/test032.$f.db $dir/test032.$f.save1
#	}
#	foreach f [glob $dir/log.*] {
#		if { [is_substr $f save] == 0 } {
#			exec $CP $f $f.save1
#		}
#	}

	# Run recovery and then read through all the database files to make
	# sure that they all look good.

	puts "\tTest032.verify: Running recovery and verifying file contents"
	set stat [catch {exec ./db_recover -v -h $dir} result]
	if { $stat == 1 && [is_substr $result \
	    "db_recover: Recovering the log"] == 0 } {
		error "FAIL: Recovery error: $result."
	}

	# Save everything away in case something breaks
#	for { set f 0 } { $f < $nfiles } {incr f} {
#		exec $CP $dir/test032.$f.db $dir/test032.$f.save2
#	}
#	foreach f [glob $dir/log.*] {
#		if { [is_substr $f save] == 0 } {
#			exec $CP $f $f.save2
#		}
#	}

	for { set f 0 } { $f < $nfiles } { incr f } {
		set db($f) \
		    [eval dbopen test032.$f.db 0 0 DB_UNKNOWN  -dbhome $dir]
		error_check_good $f:dbopen [is_valid_db $db($f)] TRUE

		set cursors($f) [$db($f) cursor 0]
		error_check_bad $f:cursor_open $cursors($f) NULL
		error_check_good $f:cursor_open [is_substr $cursors($f) $db($f)] 1
	}

	for { set f 0 } { $f < $nfiles } { incr f } {
		for {set d [$cursors($f) get 0 $DB_FIRST] } \
		    { [string length $d] != 0 } \
		    { set d [$cursors($f) get 0 $DB_NEXT] } {

			set k [lindex $d 0]
			set d [lindex $d 1]

			set flist [zero_list $nfiles]
			set r $d
			while { [set ndx [string first : $r]] != -1 } {
				set fnum [string range $r 0 [expr $ndx - 1]]
				if { [lindex $flist $fnum] == 0 } {
					set fl $DB_SET
				} else {
					set fl $DB_NEXT
				}

				if { $fl != $DB_SET || $fnum != $f } {
					set full [$cursors($fnum) get $k $fl]
					set key [lindex $full 0]
					set rec [lindex $full 1]
					error_check_good $f:dbget_$fnum:key \
					    $key $k
					error_check_good $f:dbget_$fnum:data \
					    $rec $d
				}

				set flist [lreplace $flist $fnum $fnum 1]
				incr ndx
				set r [string range $r $ndx end]
			}
		}
	}

	for { set f 0 } { $f < $nfiles } { incr f } {
		error_check_good $cursors($f) [$cursors($f) close] 0
		error_check_good db_close:$f [$db($f) close] 0
	}
}
