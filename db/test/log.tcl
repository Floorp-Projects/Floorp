# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)log.tcl	10.7 (Sleepycat) 4/21/98
#
# Options are:
# -dir <directory in which to store memp>
# -maxfilesize <maxsize of log file>
# -iterations <iterations>
# -stat
proc log_usage {} {
	puts "log -dir <directory> -iterations <number of ops> \
	    -maxfilesize <max size of log files> -stat"
}
proc logtest { args } {
	source ./include.tcl

# Set defaults
	set iterations 1000
	set maxfile [expr 1024 * 128]
	set dostat 0
	for { set i 0 } { $i < [llength $args] } {incr i} {
		switch -regexp -- [lindex $args $i] {
			-d.* { incr i; set testdir [lindex $args $i] }
			-i.* { incr i; set iterations [lindex $args $i] }
			-m.* { incr i; set maxfile [lindex $args $i] }
			-s.* { set dostat 1 }
			default {
				log_usage
				return
			}
		}
	}
	if { [file exists $testdir] != 1 } {
		exec $MKDIR $testdir
	} elseif { [file isdirectory $testdir ] != 1 } {
		error "$testdir is not a directory"
	}
	set multi_log [expr 3 * $iterations]

	# Clean out old log if it existed
	puts "Unlinking log: error message OK"
	cleanup $testdir

	# Now run the various functionality tests
	log001 $testdir $maxfile
	log002 $testdir $maxfile $iterations
	log002 $testdir $maxfile $multi_log
	log003 $testdir $maxfile
	log004 $testdir $maxfile
}

proc log001 { dir max } {
source ./include.tcl
puts "Log001: Open/Close/Create/Unlink test"
	log_unlink $testdir 1
	log_cleanup $dir

	# Try opening without Create flag should error
	set lp [ log "" 0 0 ]
	error_check_good log:fail $lp NULL

	# Now try opening with create
	set lp [ log "" $DB_CREATE 0644 -maxsize $max ]
	error_check_bad log:$dir $lp NULL
	error_check_good log:$dir [is_substr $lp log] 1

	# Make sure that close works.
	error_check_good log_close:$lp [$lp close] 0

	# Make sure we can reopen.
	set lp [ log "" 0 0 ]
	error_check_bad log:$dir $lp NULL
	error_check_good log:$dir [is_substr $lp log] 1

	# Try unlinking while we're still attached, should fail.
	error_check_good log_unlink:$dir [log_unlink $testdir 0] -1

	# Now close it and unlink it
	error_check_good log_close:$lp [$lp close] 0
	error_check_good log_unlink:$dir [log_unlink $testdir 0] 0
	log_cleanup $dir
}

proc log002 { dir max nrecs } {
source ./include.tcl
puts "Log002: Basic put/get test"

	log_unlink $testdir 1
	log_cleanup $dir

	set lp [ log "" $DB_CREATE 0644 -maxsize $max ]
	error_check_bad log:$dir $lp NULL
	error_check_good log:$dir [is_substr $lp log] 1

	# We will write records to the log and make sure we can
	# read them back correctly.  We'll use a standard pattern
	# repeated some number of times for each record.

	set lsn_list {}
	set rec_list {}
	puts "Log002.a: Writing $nrecs log records"
	for { set i 0 } { $i < $nrecs } { incr i } {
		set rec ""
		for { set j 0 } { $j < [expr $i % 10 + 1] } {incr j} {
			set rec $rec$i:logrec:$i
		}
		set lsn [$lp put $rec 0]
		error_check_bad log_put [is_substr $lsn log_cmd] 1
		lappend lsn_list $lsn
		lappend rec_list $rec
	}
	puts "Log002.b: Retrieving log records sequentially (forward)"
	set i 0
	for { set grec [$lp get {0 0} $DB_FIRST] } { [llength $grec] != 0 } {
	    set grec [$lp get {0 0} $DB_NEXT] } {
		error_check_good log_get:seq $grec [lindex $rec_list $i]
		incr i
	}

	puts "Log002.c: Retrieving log records sequentially (backward)"
	set i [llength $rec_list]
	for { set grec [$lp get {0 0} $DB_LAST] } { [llength $grec] != 0 } {
	    set grec [$lp get {0 0} $DB_PREV] } {
		incr i -1
		error_check_good log_get:seq $grec [lindex $rec_list $i]
	}

	puts "Log002.d: Retrieving log records sequentially by LSN"
	set i 0
	foreach lsn $lsn_list {
		set grec [$lp get $lsn $DB_SET]
		error_check_good log_get:seq $grec [lindex $rec_list $i]
		incr i
	}

	puts "Log002.e: Retrieving log records randomly by LSN"
	set m [expr [llength $lsn_list] - 1]
	for { set i 0 } { $i < $nrecs } { incr i } {
		set recno [random_int 0 $m ]
		set lsn [lindex $lsn_list $recno]
		set grec [$lp get $lsn $DB_SET]
		error_check_good log_get:seq $grec [lindex $rec_list $recno]
	}

	# Close and unlink the file
	error_check_good log_close:$lp [$lp close] 0
	error_check_good log_unlink:$dir [log_unlink $testdir 0] 0
	log_cleanup $dir
}

proc log003 { dir {max 32768} } {
puts "Log003: Multiple log test w/trunc, file, compare functionality"
source ./include.tcl

	log_unlink $testdir 1
	log_cleanup $dir

	set lp [ log "" $DB_CREATE 0644 -maxsize $max ]
	error_check_bad log:$dir $lp NULL
	error_check_good log:$dir [is_substr $lp log] 1

	# We'll record every hundred'th record for later use
	set info_list {}

	set i 0
	puts "Log003.a: Writing log records"
	for { set s 0 } { $s < [expr 3 * $max] } { incr s $len } {
		set rec [random_data 120 0 0]
		set len [string length $rec]
		set lsn [$lp put $rec 0]

		if { [expr $i % 100 ] == 0 } {
			lappend info_list [list $lsn $rec]
		}
		incr i
	}

	puts "Log003.b: Checking log_compare"
	set last {0 0}
	foreach p $info_list {
		set l [lindex $p 0]
		if { [llength $last] != 0 } {
			error_check_good log_compare [$lp compare $l $last] 1
			error_check_good log_compare [$lp compare $last $l] -1
			error_check_good log_compare [$lp compare $l $l] 0
		}
		set last $l
	}

	puts "Log003.c: Checking log_file"
	set flist [glob $dir/log*]
	foreach p $info_list {
		set f [$lp file [lindex $p 0]]

		# Change all backslash separators on Windows to forward slash
		# separators, which is what the rest of the test suite expects.
		regsub -all {\\} $f {/} f

		error_check_bad log_file:$f [lsearch $flist $f] -1
	}

	puts "Log003.d: Verifying records"
	for {set i [expr [llength $info_list] - 1] } { $i >= 0 } { incr i -1} {
		set p [lindex $info_list $i]
		set grec [$lp get [lindex $p 0] $DB_SET]
		error_check_good log_get:$lp $grec [lindex $p 1]
	}

	# Close and unlink the file
	error_check_good log_close:$lp [$lp close] 0
	error_check_good log_unlink:$dir [log_unlink $testdir 0] 0
	log_cleanup $dir

	puts "Test003 Complete"
}

proc log004 { dir {max 32768} } {
puts "Log004: Verify log_flush behavior"
source ./include.tcl

	log_unlink $testdir 1
	log_cleanup $dir
	set short_rec "abcdefghijklmnopqrstuvwxyz"
	set long_rec [repeat $short_rec 200]
	set very_long_rec [repeat $long_rec 4]

	foreach rec "$short_rec $long_rec $very_long_rec" {
		puts "Log004.a: Verify flush on [string length $rec] byte rec"

		set lp [ log "" $DB_CREATE 0644 -maxsize $max ]
		error_check_bad log:$dir $lp NULL
		error_check_good log:$dir [is_substr $lp log] 1

		set lsn [$lp put $rec 0]
		error_check_bad log_put [lindex $lsn 0] "ERROR:"
		set ret [$lp flush $lsn]
		error_check_good log_flush $ret 0

		# Now, we want to crash the region and recheck.  Closing the
		# log does not flush any records, so we'll use a close to
		# do the "crash"
		set ret [$lp close]
		error_check_good log_close $ret 0

		# Now, remove the log region
		set ret [log_unlink $testdir 0]
		error_check_good log_unlink $ret 0

		# Re-open the log and try to read the record.
		set lp [ log "" $DB_CREATE 0644 -maxsize $max ]
		error_check_bad log:$dir $lp NULL
		error_check_good log:$dir [is_substr $lp log] 1

		set gotrec [$lp get "0 0" $DB_FIRST]
		error_check_good lp_get $gotrec $rec

		# Close and unlink the file
		error_check_good log_close:$lp [$lp close] 0
		error_check_good log_unlink:$dir [log_unlink $testdir 0] 0
		log_cleanup $dir
	}

	foreach rec "$short_rec $long_rec $very_long_rec" {
		puts "Log004.b: Verify flush on non-last record [string length $rec]"
		set lp [ log "" $DB_CREATE 0644 -maxsize $max ]
		error_check_bad log:$dir $lp NULL
		error_check_good log:$dir [is_substr $lp log] 1

		# Put 10 random records
		for { set i 0 } { $i < 10 } { incr i} {
			set r [random_data 450 0 0]
			set lsn [$lp put $r 0]
			error_check_bad log_put [lindex $lsn 0] "ERROR:"
		}

		# Put the record we are interested in
		set save_lsn [$lp put $rec 0]
		error_check_bad log_put [lindex $save_lsn 0] "ERROR:"

		# Put 10 more random records
		for { set i 0 } { $i < 10 } { incr i} {
			set r [random_data 450 0 0]
			set lsn [$lp put $r 0]
			error_check_bad log_put [lindex $lsn 0] "ERROR:"
		}

		# Now check the flush
		set ret [$lp flush $save_lsn]
		error_check_good log_flush $ret 0

		# Now, we want to crash the region and recheck.  Closing the
		# log does not flush any records, so we'll use a close to
		# do the "crash"
		set ret [$lp close]
		error_check_good log_close $ret 0

		# Now, remove the log region
		set ret [log_unlink $testdir 0]
		error_check_good log_unlink $ret 0

		# Re-open the log and try to read the record.
		set lp [ log "" $DB_CREATE 0644 -maxsize $max ]
		error_check_bad log:$dir $lp NULL
		error_check_good log:$dir [is_substr $lp log] 1

		set gotrec [$lp get $save_lsn $DB_SET]
		error_check_good lp_get $gotrec $rec

		# Close and unlink the file
		error_check_good log_close:$lp [$lp close] 0
		error_check_good log_unlink:$dir [log_unlink $testdir 0] 0
		log_cleanup $dir
	}

	puts "Test004 Complete"
}

proc log_cleanup { dir } {
	source ./include.tcl

	set files [glob -nocomplain $dir/log.*]
	if { [llength $files] != 0} {
		foreach f $files {
			exec $RM -f $f
		}
	}
}
