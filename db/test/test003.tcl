# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test003.tcl	10.8 (Sleepycat) 4/28/98

# DB Test 3 {access method}
# Take the source files and dbtest executable and enter their names as the
# key with their contents as data.  After all are entered, retrieve all;
# compare output to original. Close file, reopen, do retrieve and re-verify.
proc test003 { method args} {
	global names
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test003: $method ($args) filename=key filecontents=data pairs"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test003.db
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
	if { [string compare $method DB_RECNO] == 0 } {
		set checkfunc test003_recno.check
	} else {
		set checkfunc test003.check
	}

	# Here is the loop where we put and get each key/data pair
	set file_list [glob ../*/*.c ./dbtest ./dbtest.exe]

	puts "\tTest003.a: put/get loop"
	set count 0
	foreach f $file_list {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
			set names([expr $count + 1]) $f
		} else {
			set key $f
		}
		$db putbin $txn $key ./$f $flags
		set ret [$db getbin $t4 $txn $key $flags]

		error_check_good Test003:diff(./$f,$t4) \
		    [catch { exec $DIFF ./$f $t4 } res] 0

		incr count
	}

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest003.b: dump file"
	dump_bin_file $db $txn $t1 $checkfunc
	error_check_good db_close [$db close] 0

	# Now compare the keys to see if they match the entries in the
	# current directory
	if { [string compare $method DB_RECNO] == 0 } {
		set oid [open $t2 w]
		for {set i 1} {$i <= $count} {set i [incr i]} {
			puts $oid $i
		}
		close $oid
		exec $MV $t1 $t3
	} else {
		set oid [open $t2.tmp w]
		foreach f $file_list {
			puts $oid $f
		}
		close $oid
		exec $SORT $t2.tmp > $t2
		exec $RM $t2.tmp
		exec $SORT $t1 > $t3
	}

	error_check_good Test003:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again.
	puts "\tTest003.c: close, open, and dump file"
	open_and_dump_file $testfile NULL $txn $t1 $checkfunc \
	    dump_bin_file_direction $DB_FIRST $DB_NEXT

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test003:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again in reverse direction.
	puts "\tTest003.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 $checkfunc \
	    dump_bin_file_direction $DB_LAST $DB_PREV

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test003:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
}

# Check function for test003; key should be file name; data should be contents
proc test003.check { binfile tmpfile } {
	source ./include.tcl

	error_check_good Test003:datamismatch(./$binfile,$tmpfile) \
	    [catch { exec $DIFF ./$binfile $tmpfile } res] 0
}
proc test003_recno.check { binfile tmpfile } {
	global names
	source ./include.tcl

	set fname $names($binfile)
	error_check_good key"$binfile"_exists [info exists names($binfile)] 1
	error_check_good Test003:datamismatch(./$fname,$tmpfile) \
	    [catch { exec $DIFF ./$fname $tmpfile } res] 0
}
