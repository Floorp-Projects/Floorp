# @(#)clean.awk	8.2 (Sleepycat) 3/22/97
# Minimize the set of function calls per routine.

BEGIN {
	o="awk.out"
}
/!START/,/!STOP/ {
	print $0 >> o
}
/!STOP/ {
	cmd = sprintf("sed -e '/START/d' -e '/STOP/d' %s | sort -u; >%s", \
	    o, o);
	print "    !START"
        system(cmd);
	print "    !STOP"
}
!/^ / {
	print $0
}
