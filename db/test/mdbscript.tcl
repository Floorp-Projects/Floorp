# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)mdbscript.tcl	10.2 (Sleepycat) 4/10/98
#
# Process script for the multi-process db tester.
# Usage: mdbscript dir file nentries iter procid procs seed
# dir: DBHOME directory
# file: db file on which to operate
# nentries: number of entries taken from dictionary
# iter: number of operations to run
# procid: this procses' id number
# procs: total number of processes running
# seed: Random number generator seed (-1 means use pid)
source ./include.tcl
source ../test/testutils.tcl
set datastr abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz
set usage "mdbscript dir file nentries iter procid procs seed"

# Verify usage
if { $argc != 7 } {
	puts stderr $usage
	exit
}

# Initialize arguments
set dir [lindex $argv 0]
set file [lindex $argv 1]
set nentries [ lindex $argv 2 ]
set iter [ lindex $argv 3 ]
set procid [ lindex $argv 4 ]
set procs [ lindex $argv 5 ]
set seed [ lindex $argv 6 ]

# Initialize seed
if { $seed == -1 } {
	set seed [pid]
}
srand $seed

puts "Beginning execution for [pid]"
puts "$dir db_home"
puts "$file database"
puts "$nentries data elements"
puts "$iter iterations"
puts "$procid process id"
puts "$procs processes"
puts "$seed seed"

flush stdout

set dbenv [dbenv -dbhome $dir -dbflags [expr $DB_INIT_LOCK | $DB_INIT_MPOOL]]
error_check_bad dbenv $dbenv NULL
error_check_good dbenv [is_substr $dbenv env] 1

set db [record dbopen $file 0 0 DB_UNKNOWN -dbenv $dbenv]
error_check_bad dbopen $db NULL
error_check_good dbopen [is_substr $db db] 1

set lm [lock_open "" 0 0 -dbenv $dbenv]
error_check_bad lockopen $lm NULL
error_check_good lockopen [is_substr $lm lockmgr] 1

# Init globals (no data)
set nkeys [db_init $db 0]
puts "Initial number of keys: $nkeys"
error_check_good db_init $nkeys $nentries

set txn 0

# On each iteration we're going to randomly pick a key.
# We'll either get it (verifying that its content is reasonable).
# Put it (using an overwrite to make the data be datastr:ID).
# Get it and do a put through the cursor, tacking our ID on to
# whatever is already there.
set getput 0
set overwrite 0
set gets 0
set dlen [string length $datastr]
for { set i 0 } { $i < $iter } { incr i } {
	set op [random_int 0 2]

	switch $op {
		0 {
			incr gets
			set k [random_int 0 [expr $nkeys - 1]]
			set key [lindex $l_keys $k]
			set rec [record $db get $txn $key 0 ]
			set rec [check_ret $db $lm $txn $rec ]
			if { $rec != 0 } {
				set partial [string range $rec 0 \
				    [expr $dlen - 1]]
				error_check_good db_get:$key $partial $datastr
			}
		}
		1 {
			incr overwrite
			set k [random_int 0 [expr $nkeys - 1]]
			set key [lindex $l_keys $k]
			set data $datastr:$procid
			set ret [record $db put $txn $key $data 0]
			set ret [check_ret $db $lm $txn $ret]
			error_check_good db_put:$key $ret 0
		}
		2 {
			incr getput
			set dbc [$db cursor 0]
			set k [random_int 0 [expr $nkeys - 1]]
			set key [lindex $l_keys $k]
			set ret [record $dbc get $key $DB_SET]
			set ret [check_ret $db $lm $txn $ret]
			if { $ret == 0 }  {
				break
			}

			set rec [lindex $ret 1]
			set partial [string range $rec 0 [expr $dlen - 1]]
			error_check_good db_cget:$key $partial $datastr
			append rec ":$procid"
			set ret [record $dbc put $key $rec $DB_CURRENT]
			set ret [check_ret $db $lm $txn $ret]
			error_check_good db_cput:$key $ret 0
			error_check_good db_cclose:$dbc [$dbc close] 0
		}
	}
	flush stdout
}

error_check_good db_close:$db [record $db close] 0

puts "[timestamp] [pid] Complete"
puts "Successful ops: $gets gets $overwrite overwrites $getput getputs"
flush stdout
