# See the file LICENSE for redistribution information.
#
# Copyright (c) 1996, 1997, 1998
#	Sleepycat Software.  All rights reserved.
#
#	@(#)parmtest.tcl	10.2 (Sleepycat) 4/10/98
#
# DB Parameter Test
# Run the numbered tests, exercising different parameter combinations.
proc parmtest { method  {start 1} {nskip 0} } {
	puts "Test011: $method"
	set iter 0
	# Big cache and small cache tests
	foreach cachesize "16777216 20480" {
		# Small page medium page and big page tests
		foreach pagesize "512 4096 65536" {
			# Byte order tests
			foreach lorder "4321 1234" {
				if { $iter >= $nskip } {
					run_method $method $start -lorder \
					    $lorder -psize $pagesize \
					    -cachesize $cachesize
				}
				incr iter
			}
		}
	}
}

proc parmtest.hash { {start 1} } {
	foreach ffactor "-1 5 10 20 100" {
		run_method $method 1 -ffactor $ffactor
	}
	foreach hash "1 2 3 4" {
		run_method $method $start -hash $hash
	}
}

proc parmtest.btree { {start 1} } {
	foreach minkey "2 4 8 16" {
		run_method $method $start -minkey $minkey
	}
}
