# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test010.tcl	10.5 (Sleepycat) 4/25/98
#
# DB Test 10 {access method}
# Use the first 10,000 entries from the dictionary.
# Insert each with self as key and data; add duplicate
# records for each.
# After all are entered, retrieve all; verify output.
# Close file, reopen, do retrieve and re-verify.
# This does not work for recno
proc test010 { method {nentries 10000} {ndups 5} {tnum 10} args } {
	set omethod $method
	set method [convert_method $method]
	set args [convert_args $method $args]
	puts "Test0$tnum: $method ($args) $nentries small dup key/data pairs"
	if { [string compare $method DB_RECNO] == 0 || \
	    [is_rbtree $omethod] == 1 } {
		puts "Test0$tnum skipping for method $omethod"
		return
	}

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test0$tnum.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir
	set args [add_to_args $DB_DUP $args]
	set db [eval [concat dbopen $testfile \
	    [expr $DB_CREATE | $DB_TRUNCATE] 0644 $method $args]]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	set flags 0
	set txn 0
	set count 0

	# Here is the loop where we put and get each key/data pair
	set dbc [$db cursor $txn]
	while { [gets $did str] != -1 && $count < $nentries } {
		for { set i 1 } { $i <= $ndups } { incr i } {
			set datastr $i:$str
			$db put $txn $str $datastr $flags
		}

		# Now retrieve all the keys matching this key
		set x 1
		for {set ret [$dbc get $str $DB_SET]} \
		    {[string length $ret] != 0} \
		    {set ret [$dbc get 0 $DB_NEXT] } {
			set k [lindex $ret 0]
			if { [string compare $k $str] != 0 } {
				break
			}
			set datastr [lindex $ret 1]
			set d [data_of $datastr]
			if {[string length $d] == 0} {
				break
			}
			error_check_good "Test0$tnum:put" $d $str
			set id [ id_of $datastr ]
			error_check_good "Test0$tnum:dup#" $id $x
			incr x
		}
		error_check_good "Test0$tnum:ndups:$str" [expr $x - 1] $ndups
		incr count
	}
	error_check_good cursor_close [$dbc close] 0
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest0$tnum.a: Checking file for correct duplicates"
	set dlist ""
	for { set i 1 } { $i <= $ndups } {incr i} {
		lappend dlist $i
	}
	dup_check $db $txn $t1 $dlist

	# Now compare the keys to see if they match the dictionary entries
	set q q
	exec $SED $nentries$q $dict > $t2
	exec $SORT $t1 > $t3

	error_check_good Test0$tnum:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	error_check_good db_close [$db close] 0
	set db [dbopen $testfile 0 0 $method]
	error_check_good dbopen [is_valid_db $db] TRUE

	puts "\tTest0$tnum.b: Checking file for correct duplicates after close"
	dup_check $db $txn $t1 $dlist

	# Now compare the keys to see if they match the dictionary entries
	exec $SORT $t1 > $t3
	error_check_good Test0$tnum:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	error_check_good db_close [$db close] 0
}
