# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test014.tcl	10.6 (Sleepycat) 4/26/98
#
# DB Test 14 {access method}
# Partial put test, small data, replacing with same size.
# The data set consists of the first nentries of the dictionary.
# We will insert them (and retrieve them) as we do in test 1 (equal
# key/data pairs).  Then we'll try to perform partial puts of some characters
# at the beginning, some at the end, and some at the middle.
proc test014 { method {nentries 10000} args } {
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test014: $method ($args) $nentries equal key/data pairs, put test"
	test014_body $method 1 1 $nentries $args
	test014_body $method 1 5 $nentries $args
	test014_body $method 2 4 $nentries $args
	test014_body $method 1 100 $nentries $args
	test014_body $method 2 10 $nentries $args
	test014_body $method 1 8000 $nentries $args
}

proc test014_body { method chars increase {nentries 10000} args } {
	set method [convert_method $method]
	puts "Replace $chars chars with string $increase times larger"
	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test014.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir
	set db [eval [concat dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE] \
	    0644 $method $args] ]
	error_check_good dbopen [is_valid_db $db] TRUE

	set flags 0
	set txn 0
	set count 0

	puts "\tTest014.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	# We will do the initial put and then three Partial Puts
	# for the beginning, middle and end of the string.
	set did [open $dict]
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
			set put putn
		} else {
			set key $str
			set put put
		}
		partial_put $put $db $txn $key $str $chars $increase
		incr count
	}
	close $did

	# Now make sure that everything looks OK
	puts "\tTest014.b: check entire file contents"
	dump_file $db $txn $t1 test014.check
	error_check_good db_close [$db close] 0

	# Now compare the keys to see if they match the dictionary (or ints)
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

	error_check_good Test014:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	puts "\tTest014.c: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile NULL $txn \
	    $t1 test014.check dump_file_direction $DB_FIRST $DB_NEXT

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test014:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\tTest014.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 \
	    test014.check dump_file_direction $DB_LAST $DB_PREV

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test014:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
}

# Check function for test014; keys and data are identical
proc test014.check { key data } {
global dvals
	error_check_good key"$key"_exists [info exists dvals($key)] 1
	error_check_good "data mismatch for key $key" $data $dvals($key)
}

