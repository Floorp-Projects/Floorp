# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test024.tcl	8.6 (Sleepycat) 4/26/98
#
# DB Test 24 {method nentries}
# Test the Btree and Record number get-by-number functionality.

proc test024 { method {nentries 10000} args} {
	set do_renumber [is_rrecno $method]
	set args [convert_args $method $args]
	set method [convert_method $method]
	set args [number_btree $method $args]
	puts "Test024: $method ($args)"

	if { [string compare $method DB_HASH] == 0 } {
		puts "Test024 skipping for method HASH"
		return
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test024.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3

	cleanup $testdir

	# Read the first nentries dictionary elements and reverse them.
	# Keep a list of these (these will be the keys).
	puts "\tTest024.a: initialization"
	set keys ""
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		lappend keys [reverse $str]
		incr count
	}
	close $did

	# Generate sorted order for the keys
	set sorted_keys [lsort $keys]
	# Create the database
	set db [eval [concat dbopen \
	    $testfile [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE

	set flags 0
	set txn 0
	puts "\tTest024.b: put/get loop"
	foreach k $keys {
		if { [string compare $method DB_RECNO] == 0 } {
			set key [lsearch $sorted_keys $k]
			incr key
		} else {
			set key $k
		}
		set ret [$db put $txn $key $k $flags]
		error_check_good put $ret 0
		set ret [$db get $txn $key $flags]
		error_check_good get $ret $k
	}

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest024.c: dump file"

	# Put sorted keys in file
	set oid [open $t1 w]
	foreach k $sorted_keys {
		puts $oid $k
	}
	close $oid

	# Instead of using dump_file; get all the keys by keynum
	set oid [open $t2 w]
	if { [string compare $method "DB_BTREE"] == 0 } {
		set flags $DB_SET_RECNO
		set do_renumber 1
	}

	for { set k 1 } { $k <= $count } { incr k } {
		set ret [$db getn $txn $k $flags]
		puts $oid $ret
		error_check_good recnum_get $ret \
		    [lindex $sorted_keys [expr $k - 1]]
	}
	close $oid
	error_check_good db_close [$db close] 0

	error_check_good Test024.c:diff($t1,$t2) \
	    [catch { exec $DIFF $t1 $t2 } res] 0

	# Now, reopen the file and run the last test again.
	puts "\tTest024.d: close, open, and dump file"
	set db [ dbopen $testfile $DB_RDONLY 0 DB_UNKNOWN ]
	error_check_good dbopen [is_valid_db $db] TRUE
	set oid [open $t2 w]
	for { set k 1 } { $k <= $count } { incr k } {
		set ret [$db getn $txn $k $flags]
		puts $oid $ret
		error_check_good recnum_get $ret \
		    [lindex $sorted_keys [expr $k - 1]]
	}
	close $oid
	error_check_good db_close [$db close] 0
	error_check_good Test024.d:diff($t1,$t2) \
	    [catch { exec $DIFF $t1 $t2 } res] 0

	# Now, reopen the file and run the last test again in reverse direction.
	puts "\tTest024.e: close, open, and dump file in reverse direction"
	set db [ dbopen $testfile $DB_RDONLY 0 DB_UNKNOWN ]
	error_check_good dbopen [is_valid_db $db] TRUE
	# Put sorted keys in file
	set rsorted ""
	foreach k $sorted_keys {
		set rsorted [concat $k $rsorted]
	}
	set oid [open $t1 w]
	foreach k $rsorted {
		puts $oid $k
	}
	close $oid

	set oid [open $t2 w]
	for { set k $count } { $k > 0 } { incr k -1 } {
		set ret [$db getn $txn $k $flags]
		puts $oid $ret
		error_check_good recnum_get $ret \
		    [lindex $sorted_keys [expr $k - 1]]
	}
	close $oid
	error_check_good db_close [$db close] 0
	error_check_good Test024.e:diff($t1,$t2) \
	    [catch { exec $DIFF $t1 $t2 } res] 0

	# Now try deleting elements and making sure they work
	puts "\tTest024.f: delete test"
	set db [ dbopen $testfile 0 0 DB_UNKNOWN ]
	error_check_good dbopen [is_valid_db $db] TRUE
	while { $count > 0 } {
		set kndx [random_int 1 $count]
		set kval [lindex $keys [expr $kndx - 1]]
		set recno [expr [lsearch $sorted_keys $kval] + 1]

		if { [string compare $method "DB_RECNO"] == 0 } {
			set ret [$db del $txn $recno 0]
		} else {
			set ret [$db del $txn $kval 0]
		}
		error_check_good delete $ret 0

		# Remove the key from the key list
		set ndx [expr $kndx - 1]
		set keys [lreplace $keys $ndx $ndx]

		if { $do_renumber == 1 } {
			set r [expr $recno - 1]
			set sorted_keys [lreplace $sorted_keys $r $r]
		}

		# Check that the keys after it have been renumbered
		if { $do_renumber == 1 && $recno != $count } {
			set r [expr $recno - 1]
			set ret [$db getn $txn $recno $flags]
			error_check_good get_after_del $ret \
				[lindex $sorted_keys $r]
		}

		# Decrement count
		incr count -1
	}
	error_check_good db_close [$db close] 0
}

