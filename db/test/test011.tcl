# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test011.tcl	10.7 (Sleepycat) 4/26/98
#
# DB Test 11 {access method}
# Use the first 10,000 entries from the dictionary.
# Insert each with self as key and data; add duplicate
# records for each.
# Then do some key_first/key_last add_before, add_after operations.
# This does not work for recno
# To test if dups work when they fall off the main page, run this with
# a very tiny page size.
proc test011 { method {nentries 10000} {ndups 5} {tnum 11} args } {
global dlist
	set dlist ""
	set omethod $method
	set do_renumber [is_rrecno $method]
	set args [convert_args $method $args]
	set method [convert_method $method]
	if { [is_rbtree $omethod] == 1 } {
		puts "Test011 skipping for method $omethod"
		return
	}
	if { [string compare $method DB_RECNO] == 0 } {
		test011_recno $do_renumber $nentries $tnum $args
		return
	} else {
		puts -nonewline "Test0$tnum: $method $nentries small dup "
		puts "key/data pairs, cursor ops"
	}
	if {$ndups < 5} {
		set ndups 5
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
	# We will add dups with values 1, 3, ... $ndups.  Then we'll add
	# 0 and $ndups+1 using keyfirst/keylast.  We'll add 2 and 4 using
	# add before and add after.
	puts "\tTest0$tnum.a: put and get duplicate keys."
	set dbc [$db cursor $txn]
	set i ""
	for { set i 1 } { $i <= $ndups } { incr i 2 } {
		lappend dlist $i
	}
	set maxodd $i
	while { [gets $did str] != -1 && $count < $nentries } {
		for { set i 1 } { $i <= $ndups } { incr i 2 } {
			set datastr $i:$str
			set ret [$db put $txn $str $datastr $flags]
			error_check_good put $ret 0
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
			error_check_good Test0$tnum:put $d $str
			set id [ id_of $datastr ]
			error_check_good Test0$tnum:dup# $id $x
			incr x 2
		}
		error_check_good Test011:numdups $x $maxodd
		incr count
	}
	error_check_good curs_close [$dbc close] 0
	close $did

	# Now we will get each key from the DB and compare the results
	# to the original.
	puts "\tTest0$tnum.b: traverse entire file checking duplicates before close."
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

	puts "\tTest0$tnum.c: traverse entire file checking duplicates after close."
	dup_check $db $txn $t1 $dlist

	# Now compare the keys to see if they match the dictionary entries
	exec $SORT $t1 > $t3
	error_check_good Test0$tnum:diff($t3,$t2) \
	    [catch { exec $DIFF $t3 $t2 } res] 0

	puts "\tTest0$tnum.d: Testing key_first functionality"
	add_dup $db $txn $nentries $DB_KEYFIRST 0 0
	set dlist [linsert $dlist 0 0]
	dup_check $db $txn $t1 $dlist

	puts "\tTest0$tnum.e: Testing key_last functionality"
	add_dup $db $txn $nentries $DB_KEYLAST [expr $maxodd - 1] 0
	lappend dlist [expr $maxodd - 1]
	dup_check $db $txn $t1 $dlist

	puts "\tTest0$tnum.f: Testing add_before functionality"
	add_dup $db $txn $nentries $DB_BEFORE 2 3
	set dlist [linsert $dlist 2 2]
	dup_check $db $txn $t1 $dlist

	puts "\tTest0$tnum.g: Testing add_after functionality"
	add_dup $db $txn $nentries $DB_AFTER 4 4
	set dlist [linsert $dlist 4 4]
	dup_check $db $txn $t1 $dlist

	error_check_good db_close [$db close] 0
}

proc add_dup {db txn nentries flag dataval iter} {
source ./include.tcl

	set dbc [$db cursor $txn]
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $nentries } {
		set datastr $dataval:$str
		set ret [$dbc get $str $DB_SET]
		error_check_bad "cget(SET)" [is_substr $ret Error] 1
		for { set i 1 } { $i < $iter } { incr i } {
			set ret [ $dbc get $str $DB_NEXT ]
			error_check_bad "cget(NEXT)" [is_substr $ret Error] 1
		}

		$dbc put $str $datastr $flag
		incr count
	}
	close $did
	$dbc close
}

proc test011_recno { {renum 0} {nentries 10000} {tnum 11} largs } {
global dlist
	puts "Test0$tnum: RECNO ($largs) $nentries test cursor insert functionality"

	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	# Create the database and open the dictionary
	set testfile test0$tnum.db
	set t1 $testdir/t1
	set t2 $testdir/t2
	set t3 $testdir/t3
	cleanup $testdir
	set db [eval [concat dbopen $testfile [expr $DB_CREATE | $DB_TRUNCATE] \
	    0644 DB_RECNO $largs]]
	error_check_good dbopen [is_valid_db $db] TRUE
	set did [open $dict]

	set flags 0
	set txn 0
	set count 0

	# The basic structure of the test is that we pick a random key
	# in the database and then add items before, after, ?? it.  The
	# trickiness is that with RECNO, these are not duplicates, they
	# are creating new keys.  Therefore, every time we do this, the
	# keys assigned to other values change.  For this reason, we'll
	# keep the database in tcl as a list and insert properly into
	# it to verify that the right thing is happening.  If we do not
	# have renumber set, then the BEFORE and AFTER calls should fail.

	# Seed the database with an initial record
	gets $did str
	set ret [$db put $txn 1 $str 0]
	error_check_good put $ret 0
	set count 1

	set dlist "NULL $str"

	# Open a cursor
	set dbc [$db cursor $txn]
	puts "Test0$tnum.a: put and get entries"
	while { [gets $did str] != -1 && $count < $nentries } {
		# Pick a random key
		set key [random_int 1 $count]
		set ret [$dbc get $key $DB_SET]
		set k [lindex $ret 0]
		set d [lindex $ret 1]
		error_check_good cget:SET:key $k $key
		error_check_good cget:SET $d [lindex $dlist $key]

		# Current
		set ret [$dbc put $key $str $DB_CURRENT]
		error_check_good cput:$key $ret 0
		set dlist [lreplace $dlist $key $key $str]

		# Before
		if { [gets $did str] == -1 } {
			continue;
		}

		if { $renum == 1 } {
			set ret [$dbc put $key $str $DB_BEFORE]
			error_check_good cput:$key:BEFORE $ret 0
			set dlist [linsert $dlist $key $str]
			incr count

			# After
			if { [gets $did str] == -1 } {
				continue;
			}
			set ret [$dbc put $key $str $DB_AFTER]
			error_check_good cput:$key:AFTER $ret 0
			set dlist [linsert $dlist [expr $key + 1] $str]
			incr count
		}

		# Now verify that the keys are in the right place
		set i 0
		for {set ret [$dbc get $key $DB_SET]} \
		    {[string length $ret] != 0 && $i < 3} \
		    {set ret [$dbc get 0 $DB_NEXT] } {
			set check_key [expr $key + $i]

			set k [lindex $ret 0]
			error_check_good cget:$key:loop $k $check_key

			set d [lindex $ret 1]
			error_check_good cget:data $d [lindex $dlist $check_key]
			incr i
		}
	}
	close $did
	error_check_good cclose [$dbc close] 0

	# Create  check key file.
	set oid [open $t2 w]
	for {set i 1} {$i <= $count} {incr i} {
		puts $oid $i
	}
	close $oid

	puts "\tTest0$tnum.b: dump file"
	dump_file $db $txn $t1 test011_check
	error_check_good Test0$tnum:diff($t2,$t1) \
	    [catch { exec $DIFF $t2 $t1 } res] 0

	error_check_good db_close [$db close] 0

	puts "\tTest0$tnum.c: close, open, and dump file"
	open_and_dump_file $testfile NULL $txn $t1 test011_check \
	    dump_file_direction $DB_FIRST $DB_NEXT
	error_check_good Test0$tnum:diff($t2,$t1) \
	    [catch { exec $DIFF $t2 $t1 } res] 0

	puts "\tTest0$tnum.d: close, open, and dump file in reverse direction"
	open_and_dump_file $testfile NULL $txn $t1 test011_check \
	    dump_file_direction $DB_LAST $DB_PREV

	exec $SORT -n $t1 > $t3
	error_check_good Test0$tnum:diff($t2,$t3) \
	    [catch { exec $DIFF $t2 $t3 } res] 0
}

proc test011_check { key data } {
global dlist
	error_check_good "get key $key" $data [lindex $dlist $key]
}
