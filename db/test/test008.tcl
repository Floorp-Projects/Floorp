# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test008.tcl	10.5 (Sleepycat) 4/28/98

# DB Test 8 {access method}
# Take the source files and dbtest executable and enter their names as the
# key with their contents as data.  After all are entered, begin looping
# through the entries; deleting some pairs and then readding them.
proc test008 { method {nentries 10000} {reopen 8} {debug 0} args} {
	set tnum Test00$reopen
	set method [convert_method $method]
	if { [string compare $method DB_RECNO] == 0 } {
		puts "Test00$reopen skipping for method RECNO"
		return
	}

	puts -nonewline "$tnum: $method filename=key filecontents=data pairs"
	if {$reopen == 9} {
		puts "(with close)"
	} else {
		puts ""
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile $tnum.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	set t4 $testdir/t4


	cleanup $testdir
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE
	set flags 0
	set txn 0

	# Here is the loop where we put and get each key/data pair
	set file_list [glob ../*/*.c ./dbtest ./dbtest.exe]

	set count 0
	puts "\tTest00$reopen.a: Initial put/get loop"
	foreach f $file_list {
		set names($count) $f
		set key $f
		$db putbin $txn $key ./$f $flags
		set ret [$db getbin $t4 $txn $key $flags]
		error_check_good Test00$reopen:diff(./$f,$t4) \
		    [catch { exec $DIFF ./$f $t4 } res] 0

		incr count
	}

	if {$reopen == 9} {
		error_check_good db_close [$db close] 0
		set db [ dbopen $testfile 0 0 DB_UNKNOWN ]
		error_check_good dbopen [is_valid_db $db] TRUE
	}

	# Now we will get step through keys again (by increments) and
	# delete all the entries, then re-insert them.

	puts "\tTest00$reopen.b: Delete re-add loop"
	foreach i "1 2 4 8 16" {
		for {set ndx 0} {$ndx < $count} { incr ndx $i} {
			set r [$db del $txn $names($ndx) $flags]
			error_check_good db_del:$names($ndx) $r 0
		}
		for {set ndx 0} {$ndx < $count} { incr ndx $i} {
			set r [$db putbin $txn $names($ndx) $names($ndx) $flags]
			error_check_good db_putbin:$names($ndx) $r 0
		}
	}

	if {$reopen == 9} {
		error_check_good db_close [$db close] 0
		set db [ dbopen $testfile 0 0 DB_UNKNOWN ]
	}

	# Now, reopen the file and make sure the key/data pairs look right.
	puts "\tTest00$reopen.c: Dump contents forward"
	dump_bin_file $db $txn $t1 test008.check

	set oid [open $t2.tmp w]
	foreach f $file_list {
		puts $oid $f
	}
	close $oid
	exec $SORT $t2.tmp > $t2
	exec $RM $t2.tmp
	exec $SORT $t1 > $t3

	error_check_good Test00$reopen:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again in reverse direction.
	puts "\tTest00$reopen.d: Dump contents backward"
	dump_bin_file_direction $db $txn $t1 test008.check $DB_LAST $DB_PREV

	exec $SORT $t1 > $t3

	error_check_good Test00$reopen:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
	error_check_good close:$db [$db close] 0
}

proc test008.check { binfile tmpfile } {
	global tnum
	source ./include.tcl

	error_check_good diff(./$binfile,$tmpfile) \
	    [catch { exec $DIFF ./$binfile $tmpfile } res] 0
}
