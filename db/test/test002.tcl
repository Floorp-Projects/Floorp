# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test002.tcl	10.6 (Sleepycat) 4/26/98
#
# DB Test 2 {access method}
# Use the first 10,000 entries from the dictionary.
# Insert each with self as key and a fixed, medium length data string;
# retrieve each. After all are entered, retrieve all; compare output
# to original. Close file, reopen, do retrieve and re-verify.

set datastr abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz

proc test002 { method {nentries 10000} args } {
	global datastr
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test002: $method ($args) $nentries key <fixed data> pairs"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test002.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	set flags 0
	set txn 0
	set count 0

	# Here is the loop where we put and get each key/data pair

	puts "\tTest002.a: put/get loop"
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
			set put putn
		} else {
			set key $str
			set put put
		}
		set ret [$db $put $txn $key $datastr $flags]
		error_check_good put $ret 0

		set ret [$db get $txn $key $flags]
		error_check_good get $ret $datastr
		incr count
	}
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest002.b: dump file"
	dump_file $db $txn $t1 test002.check
	error_check_good db_close [$db close] 0

	# Now compare the keys to see if they match the dictionary
	if { [string compare $method DB_RECNO] == 0 } {
		set oid [open $t2 w]
		for {set i 1} {$i <= $nentries} {set i [incr i]} {
			puts $oid $i
		}
		close $oid
		exec $MV $t1 $t3
	} else {
		set q q
		exec $SED $nentries$q $dict > $t2
		exec $SORT $t1 > $t3
	}

	error_check_good Test002:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again.
	puts "\tTest002.c: close, open, and dump file"
	open_and_dump_file $testfile NULL $txn $t1 test002.check \
	    dump_file_direction $DB_FIRST $DB_NEXT

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}
	error_check_good Test002:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again in reverse direction.
	puts "\tTest002.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 test002.check \
	    dump_file_direction $DB_LAST $DB_PREV

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}
	error_check_good Test002:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
}

# Check function for test002; data should be fixed are identical
proc test002.check { key data } {
	global datastr
	error_check_good "data mismatch for key $key" $data $datastr
}
