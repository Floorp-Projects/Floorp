# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)test.tcl	10.28 (Sleepycat) 5/31/98

source ./include.tcl
source ../test/testutils.tcl
source ../test/byteorder.tcl

set testdir ./TESTDIR
if { [file exists $testdir] != 1 } {
	exec $MKDIR $testdir
}

set is_windows_test 0

set parms(test001) 10000
set parms(test002) 10000
set parms(test003) ""
set parms(test004) {10000 4 0}
set parms(test005) 10000
set parms(test006) {10000 6}
set parms(test007) 10000
set parms(test008) {10000 8 0}
set parms(test009) 10000
set parms(test010) {10000 5 10}
set parms(test011) {10000 5 11}
set parms(test012)  ""
set parms(test013) 10000
set parms(test014) 10000
set parms(test015) {7500 0}
set parms(test016) 10000
set parms(test017) 10000
set parms(test018) 10000
set parms(test019) 10000
set parms(test020) 10000
set parms(test021) 10000
set parms(test022) ""
set parms(test023) ""
set parms(test024) 10000
set parms(test025) 10000
set parms(test026) {2000 5 26}
set parms(test027) {100}
set parms(test028) ""
set parms(test029) 10000

set dict ../test/wordlist
set alphabet "abcdefghijklmnopqrstuvwxyz"
set recd_debug 0

set loadtests 33
set runtests 29
set recdtests 5
set deadtests 2
set bugtests 7
set rsrctests 1
for { set i 1 } { $i <= $loadtests } {incr i} {
	set name [format "test%03d.tcl" $i]
	source ../test/$name
}
for { set i 1 } { $i <= $recdtests } {incr i} {
	set name [format "recd%03d.tcl" $i]
	source ../test/$name
}
for { set i 1 } { $i <= $deadtests } {incr i} {
	set name [format "dead%03d.tcl" $i]
	source ../test/$name
}
for { set i 1 } { $i <= $bugtests } {incr i} {
	set name [format "bug%03d.tcl" $i]
	source ../test/$name
}
for { set i 1 } { $i <= $rsrctests } {incr i} {
	set name [format "rsrc%03d.tcl" $i]
	source ../test/$name
}

source ../test/archive.tcl
source ../test/dbm.tcl
source ../test/hsearch.tcl
source ../test/lock.tcl
source ../test/mpool.tcl
source ../test/mlock.tcl
source ../test/mutex.tcl
source ../test/ndbm.tcl
source ../test/randomlock.tcl
source ../test/log.tcl
source ../test/txn.tcl

# Test driver programs

# Use args for options
proc run_method { method {start 1} {stop 0} args } {
	global parms
	global debug_print
	global debug_on
	global runtests
	if { $stop == 0 } {
		set stop $runtests
	}
	puts "run_method: $method $start $stop $args"

	for { set i $start } { $i <= $stop } {incr i} {
		puts "[timestamp]"
		set name [format "test%03d" $i]
		eval $name $method $parms($name) $args
		if { $debug_print != 0 } {
			puts ""
		}
		if { $debug_on != 0 } {
			debug
		}
		flush stdout
		flush stderr
	}
}

proc r { args } {
	global recdtests

	set l [ lindex $args 0 ]
	switch $l {
		ampool { eval mpool -shmem anon [lrange $args 1 end] }
		archive { eval archive [lrange $args 1 end] }
		byte {
			foreach method "DB_HASH DB_BTREE DB_RECNO DB_RRECNO" {
				byteorder $method
			}
		}
		dbm { eval dbm }
		dead {
			eval dead001 [lrange $args 1 end]
			eval dead002 [lrange $args 1 end]
		}
		hsearch { eval htest }
		lock { eval locktest [lrange $args 1 end] }
		log { eval logtest [lrange $args 1 end] }
		mpool { eval mpool [lrange $args 1 end] }
		nmpool { eval mpool -shmem named [lrange $args 1 end] }
		mutex { eval mutex [lrange $args 1 end] }
		ndbm { eval ndbm }
		recd {
			foreach method "DB_HASH DB_BTREE DB_RECNO DB_RRECNO" {
				for { set i 1 } {$i <= $recdtests} {incr i} {
					set name [format "recd%03d" $i]
					eval $name $method
					flush stdout
					flush stderr
				}
			}
		}
		rsrc {
			eval rsrc001
		}
		txn { eval txntest [lrange $args 1 end] }
		default { eval run_method $args }
	}
	flush stdout
	flush stderr
}

proc run_all { } {
source include.tcl
global runtests
global recdtests
	exec $RM -rf ALL.OUT
	foreach i "archive byte lock log mpool ampool nmpool mutex txn" {
		puts "Running $i tests"
		if [catch {exec ./dbtest << "r $i" >>& ALL.OUT } res] {
			set o [open ALL.OUT a]
			puts $o "FAIL: $i test"
			close $o
		}
	}

	# Add deadlock detector tests
	puts "Running deadlock detection tests."
	if [catch {exec ./dbtest << "r dead"  >>& ALL.OUT} res] {
		set o [open ALL.OUT a]
		puts $o "FAIL: deadlock detector test"
		close $o
	}


	foreach i "DB_BTREE DB_RBTREE DB_HASH DB_RECNO DB_RRECNO" {
		puts "Running $i tests"
		for { set j 1 } { $j <= $runtests } {incr j} {
			if [catch {exec ./dbtest << "run_method $i $j $j" \
			    >>& ALL.OUT } res] {
				set o [open ALL.OUT a]
				puts $o "FAIL: [format "test%03d" $j] $i"
				close $o
			}
		}
	}

	puts "Running RECNO source tests"
	if [catch {exec ./dbtest << "r rsrc" >>& ALL.OUT } res] {
		set o [open ALL.OUT a]
		puts $o "FAIL: $i test"
		close $o
	}

	# Run recovery tests
	foreach method "DB_HASH DB_BTREE DB_RECNO DB_RRECNO" {
		puts "Running recovery tests for $method"
		for { set i 1 } {$i <= $recdtests} {incr i} {
			set name [format "recd%03d" $i]
			if [catch {exec ./dbtest << "$name $method" \
			    >>& ALL.OUT } res] {
				set o [open ALL.OUT a]
				puts $o "FAIL: $name $method"
				close $o
			}
		}
	}

	# Check historic interfaces
	foreach t "dbm ndbm hsearch"  {
		if [catch {exec ./dbtest << "r $t"  >>& ALL.OUT} res] {
			set o [open ALL.OUT a]
			puts $o "FAIL: $t test"
			close $o
		}
	}

	catch { exec $SED -e /^FAIL/p -e d ALL.OUT } res
	if { [string length $res] == 0 } {
		puts "Regression Tests Succeeded"
	} else {
		puts "Regression Tests Failed; see ALL.OUT for log"
	}
}

proc convert_method { method } {
	switch $method {
		rrecno { return DB_RECNO }
		RRECNO { return DB_RECNO }
		db_rrecno { return DB_RECNO }
		DB_RRECNO { return DB_RECNO }
		rrec { return DB_RECNO }
		recno { return DB_RECNO }
		RECNO { return DB_RECNO }
		db_recno { return DB_RECNO }
		DB_RECNO { return DB_RECNO }
		rec { return DB_RECNO }
		btree { return DB_BTREE }
		BTREE { return DB_BTREE }
		db_btree { return DB_BTREE }
		DB_BTREE { return DB_BTREE }
		bt { return DB_BTREE }
		rbtree { return DB_BTREE }
		RBTREE { return DB_BTREE }
		db_rbtree { return DB_BTREE }
		DB_RBTREE { return DB_BTREE }
		rbt { return DB_BTREE }
		hash { return DB_HASH }
		HASH { return DB_HASH }
		db_hash { return DB_HASH }
		DB_HASH { return DB_HASH }
		h { return DB_HASH }
	}
}

proc is_rrecno { method } {
	set names { rrecno RRECNO db_rrecno DB_RRECNO rrec }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}

proc is_rbtree { method } {
	set names { rbtree RBTREE db_rbtree DB_RBTREE rbt }
	if { [lsearch $names $method] >= 0 } {
		return 1
	} else {
		return 0
	}
}


# If recno-with-renumbering or btree-with-renumbering is specified, then
# fix the arguments to specify the DB_RENUMBER/DB_RECNUM option for the
# -flags argument.
proc convert_args { method {largs ""} } {
source ./include.tcl
	set do_flags 0
	if { [is_rrecno $method] == 1 } {
		return [add_to_args $DB_RENUMBER $largs]
	} elseif { [is_rbtree $method] == 1 } {
		return [add_to_args $DB_RECNUM $largs]
	}
	return $largs
}

# Make sure the DB_RECNUM flag is set if we are doing btree.
proc number_btree { method {largs ""} } {
source ./include.tcl
	if { [string compare $method "DB_BTREE"] == 0 } {
		return [add_to_args $DB_RECNUM $largs]
	}
	return $largs
}

# We want to set a flag value.  Since there already might be one in
# args, we need to add to it.
proc add_to_args { flag_val {largs ""} } {
source ./include.tcl
	set ndx [lsearch $largs -flags]
	if { $ndx >= 0 } {
		# There is already a flags argument
		incr ndx
		set f [lindex $largs $ndx]
		set f [expr $f | $flag_val]
		set largs [lreplace $largs $ndx $ndx $f]
	} else {
		# There is no flags argument
		lappend largs -flags $flag_val
	}
	return $largs
}
