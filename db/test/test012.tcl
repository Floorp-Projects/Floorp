# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test012.tcl	10.6 (Sleepycat) 4/28/98

# DB Test 12 {access method}
# Take the source files and dbtest executable and enter their contents as
# the key with their names as data.  After all are entered, retrieve all;
# compare output to original. Close file, reopen, do retrieve and re-verify.
proc test012 { method args} {
	global names
	set method [convert_method $method]
	puts "Test012: $method filename=data filecontents=key pairs"
	if { [string compare $method DB_RECNO] == 0 } {
		puts "Test012 skipping for method RECNO"
		return
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test012.db
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

	puts "\tTest012.a: put/get loop"
	set count 0
	foreach f $file_list {
		$db putbinkey $txn $f $f $flags
		set ret [$db getbinkey $t4 $txn $f $flags]
		error_check_good getbinkey $ret $f
		incr count
	}

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest012.b: dump file"
	dump_binkey_file $db $txn $t1 test012.check
	error_check_good db_close [$db close] 0

	# Now compare the data to see if they match the .o and dbtest files
	set oid [open $t2.tmp w]
	foreach f $file_list {
		puts $oid $f
	}
	close $oid
	exec $SORT $t2.tmp > $t2
	exec $RM $t2.tmp
	exec $SORT $t1 > $t3

	error_check_good Test012:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again.
	puts "\tTest012.c: close, open, and dump file"
	open_and_dump_file $testfile NULL $txn $t1 test012.check \
	    dump_binkey_file_direction $DB_FIRST $DB_NEXT

	exec $SORT $t1 > $t3

	error_check_good Test012:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again in reverse direction.
	puts "\tTest012.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 test012.check\
	    dump_binkey_file_direction $DB_LAST $DB_PREV

	exec $SORT $t1 > $t3

	error_check_good Test012:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
}

# Check function for test012; key should be file name; data should be contents
proc test012.check { binfile tmpfile } {
	source ./include.tcl
	error_check_good Test012:diff($binfile,$tmpfile) \
	    [catch { exec $DIFF $binfile $tmpfile } res] 0
}
