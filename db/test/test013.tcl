# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test013.tcl	10.6 (Sleepycat) 4/26/98
#
# DB Test 13 {access method}
# Partial put and put flag testing 1.
# The data set consists of the first nentries of the dictionary.
# We will insert them (and retrieve them) as we do in test 1 (equal
# key/data pairs).  Then we'll try to overwrite them without an appropriate
# flag set (expect error).  Then we'll overwrite each one with its datum
# reversed.
proc test013 { method {nentries 10000} args } {
	set args [convert_args $method $args]
	set method [convert_method $method]
	puts "Test013: $method ($args) $nentries equal key/data pairs, put test"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test013.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir
	set db [eval [concat dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE] \
	    0644 $method $args] ]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	set flags 0
	set txn 0
	set count 0

	if { [string compare $method DB_RECNO] == 0 } {
		set checkfunc test013_recno.check
		set put putn
		global kvals
	} else {
		set checkfunc test013.check
		set put put
	}
	puts "\tTest013.a: put/get loop"
	# Here is the loop where we put and get each key/data pair
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
			set kvals($key) $str
		} else {
			set key $str
		}
		$db $put $txn $key $str $flags
		set ret [$db get $txn $key $flags]
		error_check_good get $ret $str
		incr count
	}
	close $did

	# Now we will try to overwrite each datum, but set the
	# NOOVERWRITE flag.
	puts "\tTest013.b: overwrite values with NOOVERWRITE flag."
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}
		set r [ $db $put $txn $key $str $DB_NOOVERWRITE ]
		error_check_good put [is_substr $r Error ] 1

		# Value should be unchanged.
		set ret [$db get $txn $key $flags]
		error_check_good get $ret $str
		incr count
	}
	close $did

	# Now we will replace each item with its datum capitalized.
	puts "\tTest013.c: overwrite values with capitalized datum"
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [expr $count + 1]
		} else {
			set key $str
		}
		set rstr [string toupper $str]
		set r [ $db $put $txn $key $rstr 0 ]
		error_check_good put $r 0

		# Value should be changed.
		set ret [$db get $txn $key $flags]
		error_check_good get $ret $rstr
		incr count
	}
	close $did

	# Now make sure that everything looks OK
	puts "\tTest013.d: check entire file contents"
	dump_file $db $txn $t1 $checkfunc
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

	error_check_good Test013:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	puts "\tTest013.e: close, open, and dump file"
	# Now, reopen the file and run the last test again.
	open_and_dump_file $testfile NULL $txn $t1 $checkfunc \
	    dump_file_direction $DB_FIRST $DB_NEXT

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test013:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	# Now, reopen the file and run the last test again in the
	# reverse direction.
	puts "\tTest013.f: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 $checkfunc \
	    dump_file_direction $DB_LAST $DB_PREV

	if { [string compare $method DB_RECNO] != 0 } {
		exec $SORT $t1 > $t3
	}

	error_check_good Test013:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0
}

# Check function for test013; keys and data are identical
proc test013.check { key data } {
	error_check_good "key/data mismatch for $key" $data \
	    [string toupper $key]
}

proc test013_recno.check { key data } {
	global dict
	global kvals
	error_check_good key"$key"_exists [info exists kvals($key)] 1
	error_check_good "data mismatch for $key" $data \
	    [string toupper $kvals($key)]
}
