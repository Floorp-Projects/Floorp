# @(#)delete.sed	8.5 (Sleepycat) 9/17/97
#
#Delete strings that we know aren't functions.

# Empty lines, whitespace.
/^$/d
/^[	 ]*$/d

# Function prototypes.
/^__P$/d

# C language keywords.
/^defined$/d
/^return$/d
/^sizeof$/d
/^va_end$/d
/^va_start$/d
