# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)testutils.tcl	10.12 (Sleepycat) 4/26/98
#
# Test system utilities

# open file and call dump_file to dumpkeys to tempfile
proc open_and_dump_file {
    dbname dbenv txn outfile checkfunc dump_func beg cont} {
	source ./include.tcl
	if { $dbenv == "NULL" } {
		set db [ dbopen $dbname $DB_RDONLY 0 DB_UNKNOWN ]
		error_check_good dbopen [is_valid_db $db] TRUE
	} else {
		set db [ dbopen $dbname $DB_RDONLY 0 DB_UNKNOWN -dbenv $dbenv]
		error_check_good dbopen [is_valid_db $db] TRUE
	}
	$dump_func $db $txn $outfile $checkfunc $beg $cont
	error_check_good db_close [$db close] 0
}

# Sequentially read a file and call checkfunc on each key/data pair.
# Dump the keys out to the file specified by outfile.
proc dump_file { db txn outfile checkfunc } {
	source ./include.tcl
	dump_file_direction $db $txn $outfile $checkfunc $DB_FIRST $DB_NEXT
}

proc dump_file_direction { db txn outfile checkfunc start continue } {
	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	set outf [open $outfile w]
	# Now we will get each key from the DB and dump to outfile
	set c [$db cursor $txn]
	for {set d [$c get 0 $start] } { [string length $d] != 0 } {
	    set d [$c get 0 $continue] } {
		set k [lindex $d 0]
		set d2 [lindex $d 1]
		$checkfunc $k $d2
		puts $outf $k
	}
	close $outf
	error_check_good curs_close [$c close] 0
}

proc dump_binkey_file { db txn outfile checkfunc } {
	source ./include.tcl
	dump_binkey_file_direction $db $txn $outfile $checkfunc \
	    $DB_FIRST $DB_NEXT
}
proc dump_bin_file { db txn outfile checkfunc } {
	source ./include.tcl
	dump_bin_file_direction $db $txn $outfile $checkfunc $DB_FIRST $DB_NEXT
}

proc dump_binkey_file_direction { db txn outfile checkfunc begin cont } {
	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl
	set d1 $testdir/d1

	set outf [open $outfile w]

	# Now we will get each key from the DB and dump to outfile
	set c [$db cursor $txn]

	for {set d [$c getbinkey $d1 0 $begin] } \
	    { [string length $d] != 0 } {
	    set d [$c getbinkey $d1 0 $cont] } {
		set data [lindex $d 0]
		set keyfile [lindex $d 1]
		$checkfunc $data $keyfile
		puts $outf $data
		flush $outf
	}
	close $outf
	error_check_good curs_close [$c close] 0
	exec $RM $d1
}

proc dump_bin_file_direction { db txn outfile checkfunc begin cont } {
	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl
	set d1 $testdir/d1

	set outf [open $outfile w]

	# Now we will get each key from the DB and dump to outfile
	set c [$db cursor $txn]

	for {set d [$c getbin $d1 0 $begin] } \
	    { [string length $d] != 0 } {
	    set d [$c getbin $d1 0 $cont] } {
		set k [lindex $d 0]
		set datfile [lindex $d 1]
		$checkfunc $k $datfile
		puts $outf $k
	}
	close $outf
	error_check_good curs_close [$c close] 0
	exec $RM -f $d1
}

proc make_data_str { key } {
	set datastr ""
	for {set i 0} {$i < 10} {incr i} {
		append datastr $key
	}
	return $datastr
}

proc error_check_bad { func result bad {txn 0}} {
	if { [string compare $result $bad] == 0 } {
		if { $txn != 0 } {
			$txn abort
		}
		flush stdout
		flush stderr
		error "FAIL:[timestamp] $func returned error value $bad"
	}
}

proc error_check_good { func result desired {txn 0} } {
	if { [string compare $result $desired] != 0 } {
		if { $txn != 0 } {
			$txn abort
		}
		flush stdout
		flush stderr
		error "FAIL:[timestamp] $func: expected $desired, got $result"
	}
}

# Locks have the prefix of their manager.
proc is_substr { l mgr } {
	if { [string first $mgr $l]  == -1 } {
		return 0
	} else {
		return 1
	}
}

proc release_list { l } {

	# Now release all the locks
	foreach el $l {
		set ret [ $el put ]
		error_check_good lock_put $ret 0
	}
}

proc debug { {stop 0} } {
global debug_on
global debug_print
global debug_test
	set debug_on 1
	set debug_print 1
	set debug_test $stop
}

# Check if each key appears exactly [llength dlist] times in the file with
# the duplicate tags matching those that appear in dlist.
proc dup_check { db txn tmpfile dlist } {
source ./include.tcl
	set outf [open $tmpfile w]
	# Now we will get each key from the DB and dump to outfile
	set c [$db cursor $txn]
	set lastkey ""
	set done 0
	while { $done != 1} {
		foreach did $dlist {
			set rec [$c get 0 $DB_NEXT]
			if { [string length $rec] == 0 } {
				set done 1
				break
			}
			set key [lindex $rec 0]
			set fulldata [lindex $rec 1]
			set id [id_of $fulldata]
			set d [data_of $fulldata]
			if { [string compare $key $lastkey] != 0 && \
			    $id != [lindex $dlist 0] } {
				set e [lindex $dlist 0]
				error "FAIL: \tKey $key, expected dup id $e, got $id"
			}
			error_check_good dupget $d $key
			error_check_good dupget $id $did
			set lastkey $key
		}
		if { $done != 1 } {
			puts $outf $key
		}
	}
	close $outf
	error_check_good curs_close [$c close] 0
}

# Parse duplicate data entries of the form N:data. Data_of returns
# the data part; id_of returns the numerical part
proc data_of {str} {
	set ndx [string first ":" $str]
	if { $ndx == -1 } {
		return ""
	}
	return [ string range $str [expr $ndx + 1] end]
}

proc id_of {str} {
	set ndx [string first ":" $str]
	if { $ndx == -1 } {
		return ""
	}

	return [ string range $str 0 [expr $ndx - 1]]
}

proc nop { {args} } {
	return
}

# Partial put test procedure.
# Munges a data val through three different partial puts.  Stores
# the final munged string in the dvals array so that you can check
# it later (dvals should be global).  We take the characters that
# are being replaced, make them capitals and then replicate them
# some number of times (n_add).  We do this at the beginning of the
# data, at the middle and at the end. The parameters are:
# db, txn, key -- as per usual.  Data is the original data element
# from which we are starting.  n_replace is the number of characters
# that we will replace.  n_add is the number of times we will add
# the replaced string back in.
proc partial_put { put db txn key data n_replace n_add } {
source ./include.tcl
global dvals

	# Here is the loop where we put and get each key/data pair
	# We will do the initial put and then three Partial Puts
	# for the beginning, middle and end of the string.

	$db $put $txn $key $data 0

	# Beginning change
	set s [string range $data 0 [ expr $n_replace - 1 ] ]
	set repl [ replicate [string toupper $s] $n_replace ]
	set newstr $repl[string range $data $n_replace end]

	set ret [$db $put $txn $key $repl $DB_DBT_PARTIAL 0 $n_replace]
	error_check_good put $ret 0

	set ret [$db get $txn $key 0]
	error_check_good get $ret $newstr

	# End Change
	set len [string length $newstr]
	set s [string range $newstr [ expr $len - $n_replace ] end ]
	set repl [ replicate [string toupper $s] $n_replace ]
	set newstr [string range $newstr 0 [expr $len - $n_replace - 1 ] ]$repl

	set ret [$db $put $txn $key $repl $DB_DBT_PARTIAL \
	    [expr $len - $n_replace] $n_replace ]
	error_check_good put $ret 0

	set ret [$db get $txn $key 0]
	error_check_good get $ret $newstr

	# Middle Change

	set len [string length $newstr]
	set mid [expr $len / 2 ]
	set beg [expr $mid - [expr $n_replace / 2] ]
	set end [expr $beg + $n_replace - 1]
	set s [string range $newstr $beg $end]
	set repl [ replicate [string toupper $s] $n_replace ]
	set newstr [string range $newstr 0 [expr $beg - 1 ] ]$repl[string range $newstr [expr $end + 1] end]

	set ret [$db $put $txn $key $repl $DB_DBT_PARTIAL $beg $n_replace]
	error_check_good put $ret 0

	set ret [$db get $txn $key 0]
	error_check_good get $ret $newstr

	set dvals($key) $newstr
}

proc replicate { str times } {
	set res $str
	for { set i 1 } { $i < $times } { set i [expr $i * 2] } {
		append res $res
	}
	return $res
}

proc isqrt { l } {
	set s [expr sqrt($l)]
	set ndx [expr [string first "." $s] - 1]
	return [string range $s 0 $ndx]
}

proc watch_procs { l {delay 30} } {
	source ./include.tcl

	while { 1 } {
		set rlist {}
		foreach i $l {
			set r [ catch { exec $KILL -0 $i } result ]
			if { $r == 0 } {
				lappend rlist $i
			}
		}
		if { [ llength $rlist] == 0 } {
			break
		} else {
			puts "[timestamp] processes running: $rlist"
		}
		exec $SLEEP $delay
	}
	puts "All processes have exited."
}

# These routines are all used from within the dbscript.tcl tester.
proc db_init { dbp do_data } {
global a_keys
global l_keys
	set txn 0
	set nk 0
	set lastkey ""
	source ./include.tcl

	set a_keys() BLANK
	set l_keys ""

	set c [$dbp cursor 0]
	for {set d [$c get 0 $DB_FIRST] } { [string length $d] != 0 } {
	    set d [$c get 0 $DB_NEXT] } {
		set k [lindex $d 0]
		set d2 [lindex $d 1]
		incr nk
		if { $do_data == 1 } {
			if { [info exists a_keys($k)] } {
				lappend a_keys($k) $d2]
			} else {
				set a_keys($k) $d2
			}
		}

		lappend l_keys $k
	}
	error_check_good curs_close [$c close] 0

	return $nk
}

proc pick_op { min max n } {
	if { $n == 0 } {
		return add
	}

	set x [random_int 1 12]
	if {$n < $min} {
		if { $x <= 4 } {
			return put
		} elseif { $x <= 8} {
			return get
		} else {
			return add
		}
	} elseif {$n >  $max} {
		if { $x <= 4 } {
			return put
		} elseif { $x <= 8 } {
			return get
		} else {
			return del
		}

	} elseif { $x <= 3 } {
		return del
	} elseif { $x <= 6 } {
		return get
	} elseif { $x <= 9 } {
		return put
	} else {
		return add
	}
}

# random_data: Generate a string of random characters.  Use average
# to pick a length between 1 and 2 * avg.  If the unique flag is 1,
# then make sure that the string is unique in the array "where"
proc random_data { avg unique where } {
upvar #0 $where arr
global debug_on
	set min 1
	set max [expr $avg+$avg-1]

	while {1} {
		set len [random_int $min $max]
		set s ""
		for {set i 0} {$i < $len} {incr i} {
			append s [int_to_char [random_int 0 25]]
		}
		if { $unique == 0 || [info exists arr($s)] == 0 } {
			break
		}
	}

	return $s
}

proc random_key { } {
global l_keys
global nkeys
	set x [random_int 0 [expr $nkeys - 1]]
	return [lindex $l_keys $x]
}

proc is_err { desired } {
	set x [random_int 1 100]
	if { $x <= $desired } {
		return 1
	} else {
		return 0
	}
}

proc pick_cursput { } {
	set x [random_int 1 4]
	switch $x {
		1 { return $DB_KEYLAST }
		2 { return $DB_KEYFIRST }
		3 { return $DB_BEFORE }
		4 { return $DB_AFTER }
	}
}

proc random_cursor { curslist } {
global l_keys
global nkeys

	set x [random_int 0 [expr [llength $curslist] - 1]]
	set dbc [lindex $curslist $x]

	# We want to randomly set the cursor.  Pick a key.
	set k [random_key]
	set r [$dbc get $k $DB_SET]
	error_check_good cursor_get:$k [is_substr Error $r] 0

	# Now move forward or backward some hops to randomly
	# position the cursor.
	set dist [random_int -10 10]

	set dir $DB_NEXT
	set boundary $DB_FIRST
	if { $dist < 0 } {
		set dir $DB_PREV
		set boundary $DB_LAST
		set dist [expr 0 - $dist]
	}

	for { set i 0 } { $i < $dist } { incr i } {
		set r [ record $dbc get $k $dir ]
		if { [llength $d] == 0 } {
			set r [ record $dbc get $k $boundary ]
		}
		error_check_bad dbcget [llength $r] 0
	}
	return { [linsert r 0 $dbc] }
}

proc record { args } {
	puts $args
	flush stdout
	return [eval $args]
}

proc newpair { k data } {
global l_keys
global a_keys
global nkeys
	set a_keys($k) $data
	lappend l_keys $k
	incr nkeys
}

proc rempair { k } {
global l_keys
global a_keys
global nkeys
	unset a_keys($k)
	set n [lsearch $l_keys $k]
	error_check_bad rempair:$k $n -1
	set l_keys [lreplace $l_keys $n $n]
	incr nkeys -1
}

proc changepair { k data } {
global l_keys
global a_keys
global nkeys
	set a_keys($k) $data
}

proc changedup { k olddata newdata } {
global l_keys
global a_keys
global nkeys
	set d $a_keys($k)
	error_check_bad changedup:$k [llength $d] 0

	set n [lsearch $d $olddata]
	error_check_bad changedup:$k $n -1

	set a_keys($k) [lreplace $a_keys($k) $n $n $newdata]
}

proc adddup { k olddata newdata where } {
global l_keys
global a_keys
global nkeys
	set d $a_keys($k)
	if { [llength $d] == 0 } {
		lappend l_keys $k
		incr nkeys
		set a_keys($k) { $newdata }
	}

	switch $where {
		case $DB_KEYFIRST { set ndx 0 }
		case $DB_KEYLAST { set ndx [llength $d] }
		case $DB_KEYBEFORE { set ndx [lsearch $d $newdata] }
		case $DB_KEYAFTER { set ndx [expr [lsearch $d $newdata] + 1]}
		default { set ndx -1 }
	}
	if { $ndx == -1 } {
		set ndx 0
	}
	set d [linsert d $ndx $newdata]
	set a_keys($k) $d
}

proc remdup { k data } {
global l_keys
global a_keys
global nkeys
	set d [$a_keys($k)]
	error_check_bad changedup:$k [llength $d] 0

	set n [lsearch $d $olddata]
	error_check_bad changedup:$k $n -1

	set a_keys($k) [lreplace $a_keys($k) $n $n]
}

proc dump_full_file { db txn outfile checkfunc start continue } {
	# Get global declarations since tcl doesn't support
	# any useful equivalent to #defines!
	source ./include.tcl

	set outf [open $outfile w]
	# Now we will get each key from the DB and dump to outfile
	set c [$db cursor $txn]
	for {set d [$c get 0 $start] } { [string length $d] != 0 } {
	    set d [$c get 0 $continue] } {
		set k [lindex $d 0]
		set d2 [lindex $d 1]
		$checkfunc $k $d2
		puts $outf "$k\t$d2"
	}
	close $outf
	error_check_good curs_close [$c close] 0
}

proc int_to_char { i } {
global alphabet
	return [string index $alphabet $i]
}

proc dbcheck { key data } {
global l_keys
global a_keys
global nkeys
global check_array

	if { [lsearch $l_keys $key] == -1 } {
		error "FAIL: Key |$key| not in list of valid keys"
	}

	set d $a_keys($key)

	if { [info exists check_array($key) ] } {
		set check $check_array($key)
	} else {
		set check {}
	}

	if { [llength $d] > 1 } {
		if { [llength $check] != [llength $d] } {
			# Make the check array the right length
			for { set i [llength $check] } { $i < [llength $d} \
			    {incr i} {
				lappend check 0
			}
			set check_array($key) $check
		}

		# Find this data's index
		set ndx [lsearch $d $data]
		if { $ndx == -1 } {
			error "FAIL: Data |$data| not found for key $key.  Found |$d|"
		}

		# Set the bit in the check array
		set check_array($key) [lreplace $check_array($key) $ndx $ndx 1]
	} elseif { [string compare $d $data] != 0 } {
		error "FAIL: Invalid data |$data| for key |$key|. Expected |$d|."
	} else {
		set check_array($key) 1
	}
}

# Dump out the file and verify it
proc filecheck { file txn } {
source ./include.tcl
global check_array
global l_keys
global nkeys
global a_keys
	if { [info exists check_array] == 1 } {
		unset check_array
	}
	open_and_dump_file $file NULL $txn $file.dump dbcheck dump_full_file \
	    $DB_FIRST $DB_NEXT

	# Check that everything we checked had all its data
	foreach i [array names check_array] {
		set count 0
		foreach j $check_array($i) {
			if { $j != 1 } {
				puts -nonewline "Key |$i| never found datum"
				puts " [lindex $a_keys($i) $count]"
			}
			incr count
		}
	}

	# Check that all keys appeared in the checked array
	set count 0
	foreach k $l_keys {
		if { [info exists check_array($k)] == 0 } {
			puts "filecheck: key |$k| not found.  Data: $a_keys($k)"
		}
		incr count
	}

	if { $count != $nkeys } {
		puts "filecheck: Got $count keys; expected $nkeys"
	}
}

proc esetup { dir } {
	source ./include.tcl

	memp_unlink $dir 1
	lock_unlink $dir 1
	exec $RM -rf $dir/file0 $dir/file1 $dir/file2 $dir/file3
	set mp [memp $dir 0644 $DB_CREATE -cachesize 10240]
	set lp [lock_open "" $DB_CREATE 0644]
	error_check_good memp_close [$mp close] 0
	error_check_good lock_close [$lp close] 0
}

proc cleanup { dir } {
source ./include.tcl
	# Remove the database and environment.
	txn_unlink $dir 1
	memp_unlink $dir 1
	log_unlink $dir 1
	lock_unlink $dir 1
	set ret [catch { glob $dir/* } result]
	if { $ret == 0 } {
		eval exec $RM -rf $result
	}
}

proc help { cmd } {
	if { [info command $cmd] == $cmd } {
		set is_proc [lsearch [info procs $cmd] $cmd]
		if { $is_proc == -1 } {
			# Not a procedure; must be a C command
			# Let's hope that it takes some parameters
			# and that it prints out a message
			puts "Usage: [eval $cmd]"
		} else {
			# It is a tcl procedure
			puts -nonewline "Usage: $cmd"
			set args [info args $cmd]
			foreach a $args {
				set is_def [info default $cmd $a val]
				if { $is_def != 0 } {
					# Default value
					puts -nonewline " $a=$val"
				} elseif {$a == "args"} {
					# Print out flag values
					puts " options"
					args
				} else {
					# No default value
					puts -nonewline " $a"
				}
			}
			puts ""
		}
	} else {
		puts "$cmd is not a command"
	}
}

# Run a recovery test for a particular operation
# Notice that we catch the return from CP and do not do anything with it.
# This is because Solaris CP seems to exit non-zero on occasion, but
# everything else seems to run just fine.
proc op_recover { op dir env_cmd dbfile cmd msg } {
source ./include.tcl
global recd_debug
global recd_id
global recd_op
	set init_file $dir/t1
	set afterop_file $dir/t2
	set final_file $dir/t3

	puts "\t$msg $op"

	# Save the initial file and open the environment and the file
	catch { exec $CP $dir/$dbfile $dir/$dbfile.init } res
	set env [eval $env_cmd]
	set nolock_env [$env simpledup]
	set tmgr [txn "" 0 0 -dbenv $env]
	set db [dbopen $dbfile 0 0 DB_UNKNOWN -dbenv $env]
	error_check_good dbopen [is_valid_db $db] TRUE

	# Dump out file contents for initial case
	open_and_dump_file $dbfile $env 0 $init_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT

	set txn [$tmgr begin]
	error_check_bad txn_begin $txn NULL
	error_check_good txn_begin [is_substr $txn $tmgr] 1

	# Now fill in the db and the txnid in the command
	set i [lsearch $cmd TXNID]
	if { $i != -1 } {
		set exec_cmd [lreplace $cmd $i $i $txn]
	} else {
		set exec_cmd $cmd
	}
	set i [lsearch $exec_cmd DB]
	if { $i != -1 } {
		set exec_cmd [lreplace $exec_cmd $i $i $db]
	} else {
		set exec_cmd $exec_cmd
	}

	# Execute command and commit/abort it.
	set ret [eval $exec_cmd]
	error_check_good "\"$exec_cmd\"" $ret 0

	# Sync the file so that we can capture a snapshot to test
	# recovery.
	error_check_good sync:$db [$db sync 0] 0
	catch { exec $CP $dir/$dbfile $dir/$dbfile.afterop } res
	open_and_dump_file $dbfile.afterop $nolock_env 0 $afterop_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT
	error_check_good txn_$op:$txn [$txn $op] 0

	if { $op == "commit" } {
		puts "\t\tCommand executed and committed."
	} else {
		puts "\t\tCommand executed and aborted."
	}

	# Dump out file and save a copy.
	error_check_good sync:$db [$db sync 0] 0
	open_and_dump_file $dbfile $nolock_env 0 $final_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT
	catch { exec $CP $dir/$dbfile $dir/$dbfile.final } res

	# If this is an abort, it should match the original file.
	# If this was a commit, then this file should match the
	# afterop file.
	if { $op == "abort" } {
		exec $SORT $init_file > $init_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(initial,post-$op):diff($init_file,$final_file) \
		    [catch { exec $DIFF $init_file.sort $final_file.sort } res] 0
	} else {
		exec $SORT $afterop_file > $afterop_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(post-$op,pre-commit):diff($afterop_file,$final_file) \
		    [catch { exec $DIFF $afterop_file.sort $final_file.sort } res] 0
	}

	# Running recovery on this database should not do anything.
	# Flush all data to disk, close the environment and save the
	# file.
	error_check_good close:$db [$db close] 0
	error_check_good txn_close [$tmgr close] 0
	reset_env $env

	debug_check
	puts -nonewline "\t\tAbout to run recovery ... "
	flush stdout

	set stat [catch {exec ./db_recover -h $dir -c} result]
	if { $stat == 1 &&
	    [is_substr $result "db_recover: Recovering the log"] == 0 } {
		error "FAIL: Recovery error: $result."
	}
	puts "complete"

	set env [eval $env_cmd]
	set nolock_env [$env simpledup]
	open_and_dump_file $dbfile $nolock_env 0 $final_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT
	if { $op == "abort" } {
		exec $SORT $init_file > $init_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(initial,post-$op):diff($init_file,$final_file) \
		    [catch { exec $DIFF $init_file.sort $final_file.sort } res] 0
	} else {
		exec $SORT $afterop_file > $afterop_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(pre-commit,post-$op):diff($afterop_file,$final_file) \
		    [catch { exec $DIFF $afterop_file.sort $final_file.sort } res] 0
	}

	# Now close the environment, substitute a file that will need
	# recovery and try running recovery again.
	reset_env $env
	if { $op == "abort" } {
		catch { exec $CP $dir/$dbfile.afterop $dir/$dbfile } res
	} else {
		catch { exec $CP $dir/$dbfile.init $dir/$dbfile } res
	}
	debug_check
	puts -nonewline \
	    "\t\tAbout to run recovery on pre-operation database ... "
	flush stdout

	set stat [catch {exec ./db_recover -h $dir -c} result]
	if { $stat == 1 &&
	    [is_substr $result "db_recover: Recovering the log"] == 0 } {
		error "FAIL: Recovery error: $result."
	}
	puts "complete"

	set env [eval $env_cmd]
	set nolock_env [$env simpledup]
	open_and_dump_file $dbfile $nolock_env 0 $final_file nop \
	    dump_file_direction $DB_FIRST $DB_NEXT
	if { $op == "abort" } {
		exec $SORT $init_file > $init_file.sort
		exec $SORT $final_file > $final_file.sort
		error_check_good \
		    diff(initial,post-$op):diff($init_file,$final_file) \
		    [catch { exec $DIFF $init_file.sort $final_file.sort } res] 0
	} else {
		exec $SORT $final_file > $final_file.sort
		exec $SORT $afterop_file > $afterop_file.sort
		error_check_good \
		    diff(post-$op,recovered):diff($afterop_file,$final_file) \
		    [catch { exec $DIFF $afterop_file.sort $final_file.sort } res] 0
	}

	# This should just close the environment, not blow it away.
	reset_env $env
}

proc populate { db method txn n dups bigdata } {
source ./include.tcl
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $n } {
		if { $method == "DB_RECNO" } {
			set key [expr $count + 1]
		} elseif { $dups == 1 } {
			set key duplicate_key
		} else {
			set key $str
		}
		if { $bigdata == 1 && [random_int 1 3] == 1} {
			set str [replicate $str 1000]
		}

		set ret [$db put $txn $key $str 0]
		error_check_good db_put:$key $ret 0
		incr count
	}
	close $did
	return 0
}

proc big_populate { db txn n } {
source ./include.tcl
	set did [open $dict]
	set count 0
	while { [gets $did str] != -1 && $count < $n } {
		set key [replicate $str 50]
		set ret [$db put $txn $key $str 0]
		error_check_good db_put:$key $ret 0
		incr count
	}
	close $did
	return 0
}

proc unpopulate { db txn num } {
source ./include.tcl
	set c [$db cursor $txn]
	error_check_bad $db:cursor $c NULL
	error_check_good $db:cursor [is_substr $c $db] 1

	set i 0
	for {set d [$c get 0 $DB_FIRST] } { [string length $d] != 0 } {
	    set d [$c get 0 $DB_NEXT] } {
		$c del 0
		incr i
		if { $num != 0 && $ >= $num } {
			break
		}
	}
	error_check_good cursor_close [$c close] 0
	return 0
}

proc reset_env { env } {
	rename $env {}
}

proc check_ret { db l txn ret } {
source ./include.tcl
	if { $ret == -1 } {
		if { $txn != 0 } {
			puts "Aborting $txn"
			return [$txn abort]
		} else {
			puts "Unlocking all for [$db locker]"
			return [$l vec [$db locker] 0 "0 0 $DB_LOCK_PUT_ALL"]
		}
	} else {
		return $ret
	}
}
# This routine will let us obtain a ring of deadlocks.
# Each locker will get a lock on obj_id, then sleep, and
# then try to lock (obj_id + 1) % num.
# When the lock is finally granted, we release our locks and
# return 1 if we got both locks and DEADLOCK if we deadlocked.
# The results here should be that 1 locker deadlocks and the
# rest all finish successfully.
proc ring { lm locker_id obj_id num } {
source ./include.tcl
	set lock1 [$lm get $locker_id $obj_id $DB_LOCK_WRITE 0]
	error_check_bad lockget:$obj_id $lock1 NULL
	error_check_good lockget:$obj_id [is_substr $lock1 $lm] 1

	exec $SLEEP 4
	set nextobj [expr ($obj_id + 1) % $num]
	set lock2 [$lm get $locker_id $nextobj $DB_LOCK_WRITE 0]

	# Now release the first lock
	error_check_good lockput:$lock1 [$lock1 put] 0

	if { $lock2 == "DEADLOCK" } {
		return DEADLOCK
	} else {
		error_check_bad lockget:$obj_id $lock2 NULL
		error_check_good lockget:$obj_id [is_substr $lock2 $lm] 1
		error_check_good lockput:$lock2 [$lock2 put] 0
		return 1
	}

}

# This routine will create massive deadlocks.
# Each locker will get a readlock on obj_id, then sleep, and
# then try to upgrade the readlock to a write lock.
# When the lock is finally granted, we release our first lock and
# return 1 if we got both locks and DEADLOCK if we deadlocked.
# The results here should be that 1 locker succeeds in getting all
# the locks and everyone else deadlocks.
proc clump { lm locker_id obj_id num } {
source ./include.tcl
	set obj_id 10
	set lock1 [$lm get $locker_id $obj_id $DB_LOCK_READ 0]
	error_check_bad lockget:$obj_id $lock1 NULL
	error_check_good lockget:$obj_id [is_substr $lock1 $lm] 1

	exec $SLEEP 4
	set lock2 [$lm get $locker_id $obj_id $DB_LOCK_WRITE 0]

	# Now release the first lock
	error_check_good lockput:$lock1 [$lock1 put] 0

	if { $lock2 == "DEADLOCK" } {
		return DEADLOCK
	} else {
		error_check_bad lockget:$obj_id $lock2 NULL
		error_check_good lockget:$obj_id [is_substr $lock2 $lm] 1
		error_check_good lockput:$lock2 [$lock2 put] 0
		return 1
	}

}

proc dead_check { t procs dead clean other } {
	error_check_good $t:$procs:other $other 0
	switch $t {
		ring {
			error_check_good $t:$procs:deadlocks $dead 1
			error_check_good $t:$procs:success $clean \
			    [expr $procs - 1]
		}
		clump {
			error_check_good $t:$procs:deadlocks $dead \
			    [expr $procs - 1]
			error_check_good $t:$procs:success $clean 1
		}
		default {
			error "Test $t not implemented"
		}
	}
}
proc rdebug { id op where } {
global recd_debug
global recd_id
global recd_op
	set recd_debug $where
	set recd_id $id
	set recd_op $op
}

proc rtag { msg id } {
	set tag [lindex $msg 0]
	set tail [expr [string length $tag] - 2]
	set tag [string range $tag $tail $tail]
	if { $id == $tag } {
		return 1
	} else {
		return 0
	}
}

proc zero_list { n } {
	set ret ""
	while { $n > 0 } {
		lappend ret 0
		incr n -1
	}
	return $ret
}

proc check_dump { k d } {
	puts "key: $k data: $d"
}

proc lock_cleanup { dir } {
	source ./include.tcl

	exec $RM -rf $dir/__db_lock.share
}
# This is defined internally in 7.4 and later versions, but since we have
# to write it 7.3 and earlier, it's easier to use it everywhere.
proc my_subst { l } {
	set ret ""
	foreach i $l {
		if {[string range $i 0 0] == "$"} {
			set v [string range $i 1 end]
			upvar $v q
			lappend ret [set q]
		} else {
			lappend ret $i
		}
	}
	return $ret
}

proc reverse { s } {
	set res ""
	for { set i 0 } { $i < [string length $s] } { incr i } {
		set res "[string index $s $i]$res"
	}

	return $res
}

proc is_valid_widget { w expected } {
	# First N characters must match "expected"
	set l [string length $expected]
	incr l -1
	if { [string compare [string range $w 0 $l] $expected] != 0 } {
		puts "case 1"
		return $w
	}

	# Remaining characters must be digits
	incr l 1
	for { set i $l } { $i < [string length $w] } { incr i} {
		set c [string index $w $i]
		if { $c < "0" || $c > "9" } {
			return $w
		}
	}

	return TRUE
}
proc is_valid_db { db } {
	return [is_valid_widget $db db]
}
