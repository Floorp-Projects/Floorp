# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)mpoolscript.tcl	10.4 (Sleepycat) 4/10/98
#
# Random multiple process mpool tester.
# Usage: mpoolscript dir id numiters numfiles numpages sleepint seed
# dir: lock directory.
# id: Unique identifier for this process.
# maxprocs: Number of procs in this test.
# numiters: Total number of iterations.
# pgsizes: Pagesizes for the different files.  Length of this item indicates
#		how many files to use.
# numpages: Number of pages per file.
# sleepint: Maximum sleep interval.
# seed: Seed for random number generator.  If -1, use pid.
# envopt: Environment options.
source ../test/testutils.tcl
source ./include.tcl

set usage \
   "mpoolscript dir id maxprocs numiters pgsizes numpages sleepint seed envopts"

# Verify usage
if { $argc != 9 } {
	puts stderr $usage
	puts $argc
	exit
}

# Initialize arguments
set dir [lindex $argv 0]
set id [lindex $argv 1]
set maxprocs [lindex $argv 2]
set numiters [ lindex $argv 3 ]
set pgsizes [ lindex $argv 4 ]
set numpages [ lindex $argv 5 ]
set sleepint [ lindex $argv 6 ]
set seed [ lindex $argv 7 ]
set envopts [ lindex $argv 8]

# Initialize seed
if { $seed == -1 } {
	srand [pid]
} else {
	srand $seed
}

puts -nonewline "Beginning execution for $id: $maxprocs $dir $numiters"
puts " $pgsizes $numpages $sleepint $seed"
flush stdout

# Figure out how small/large to make the cache
set max 0
foreach i $pgsizes {
	if { $i > $max } {
		set max $i
	}
}
set cache [expr $maxprocs * ([lindex $pgsizes 0] + $max)]

set cmd {memp "" 0 0 -dbhome $dir -cachesize $cache}
set cmd [concat $cmd $envopts]
set mp [ eval $cmd]
error_check_bad memp $mp NULL
error_check_good memp [is_substr $mp mp] 1

# Now open files
set flist {}
set nfiles 0
foreach psize $pgsizes {
	set mpf [$mp open file$nfiles $psize $DB_CREATE 0644]
	error_check_bad memp_fopen:$nfiles $mpf NULL
	error_check_good memp_fopen:$nfiles [is_substr $mpf $mp] 1
	lappend flist $mpf
	incr nfiles
}

# Now create the lock region
# We'll use the convention that we lock by fileid:pageno
set lp [lock_open "" 0 0 -dbhome $dir $envopts]
error_check_bad lock_open $lp NULL
error_check_good lock_open [is_substr $lp lockmgr] 1

puts "Establishing long-term pin on file 0 page $id for process $id"

# Set up the long-pin page
set lock [$lp get $id 0:$id $DB_LOCK_WRITE 0]
error_check_bad lock_get $lock NULL
error_check_good lock_get [is_substr $lock $lp] 1

set mpf [lindex $flist 0]
set master_page [$mpf get $id $DB_MPOOL_CREATE]
error_check_bad mp_get:$master_page $master_page NULL
error_check_good mp_get:$master_page [is_substr $master_page page] 1

set r [$master_page init MASTER$id]
error_check_good page_init $r 0

# Release the lock but keep the page pinned
set r [$lock put]
error_check_good lock_put $r 0

# Main loop.  On each iteration, we'll check every page in each of
# of the files.  On any file, if we see the appropriate tag in the
# field, we'll rewrite the page, else we won't.  Keep track of
# how many pages we actually process.
set pages 0
for { set iter 0 } { $iter < $numiters } { incr iter } {
	puts "[timestamp]: iteration $iter, $pages pages set so far"
	flush stdout
	for { set fnum 1 } { $fnum < $nfiles } { incr fnum } {
		if { [expr $fnum % 2 ] == 0 } {
			set pred [expr ($id + $maxprocs - 1) % $maxprocs]
		} else {
			set pred [expr ($id + $maxprocs + 1) % $maxprocs]
		}

		set mpf [lindex $flist $fnum]
		for { set p 0 } { $p < $numpages } { incr p } {
			set lock [$lp get $id $fnum:$p $DB_LOCK_WRITE 0]
			error_check_bad lock_get:$fnum:$p $lock NULL
			error_check_good lock_get:$fnum:$p \
			    [is_substr $lock $lp] 1

			# Now, get the page
			set pp [$mpf get $p $DB_MPOOL_CREATE]
			error_check_bad page_get:$fnum:$p $pp NULL
			error_check_good page_get:$fnum:$p \
			    [is_substr $pp page] 1

			if { [$pp check $pred] == 0 || [$pp check nul] == 0 } {
				# Set page to self.
				set r [$pp init $id]
				error_check_good page_init:$fnum:$p $r 0
				incr pages
				set r [$pp put $DB_MPOOL_DIRTY]
				error_check_good page_put:$fnum:$p $r 0
			} else {
				error_check_good page_put:$fnum:$p [$pp put 0] 0
			}
			error_check_good lock_put:$fnum:$p [$lock put] 0
		}
	}
	exec $SLEEP [random_int 1 $sleepint]
}

# Now verify your master page, release its pin, then verify everyone else's
puts "$id: End of run verification of master page"
set r [$master_page check MASTER$id]
error_check_good page_check $r 0
set r [$master_page put $DB_MPOOL_DIRTY]
error_check_good page_put $r 0

set i [expr ($id + 1) % $maxprocs]
set mpf [lindex $flist 0]
while { $i != $id } {
	set p [ $mpf get $i $DB_MPOOL_CREATE ]
	error_check_bad mp_get $p NULL
	error_check_good mp_get [is_substr $p page] 1

	if { [$p check MASTER$i] != 0 } {
		puts "Warning: Master page $i not set."
	}
	error_check_good page_put:$p [$p put 0] 0

	set i [expr ($i + 1) % $maxprocs]
}

# Close lock system
set r [$lp close]
error_check_good lock_close $r 0

# Close files
foreach i $flist {
	set r [$i close]
	error_check_good mpf_close $r 0
}

# Close mpool system
set r [$mp close]
error_check_good memp_close $r 0

puts "[timestamp] $id Complete"
flush stdout
