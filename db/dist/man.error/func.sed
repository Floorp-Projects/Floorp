# @(#)func.sed	10.2 (Sleepycat) 9/17/97

# Delete anything that looks like a comment.  (We get screwed by comments
# that list calls to functions that aren't used by the current function.)
/^ \* /d
/^[	 ][	 ]*\* /d
/^[	 ][	 ]*\/\* /d

# Surround all function call strings in the source code with ^A^BXXX^B^A,
# and bracket each function from which they're called with ^A^B@START^B^A
# and ^A^B@STOP^B^A.
#
# The ^A characters are used to ensure that we have can tokenize the
# strings and each function call will be distinct.
#
# The ^B characters are used to identify the strings we want.
#
# The @ characters are used to ensure that we don't delete START and STOP
# because they look like macro names.
#
# The sed expression is repeated because using a global flag doesn't get
# embedded calls correct.
s/\([A-Za-z_][->A-Za-z_0-9]*\)\(([^0123456789]\)/\1\2/
s/\([A-Za-z_][->A-Za-z_0-9]*\)\(([^0123456789]\)/\1\2/
s/\([A-Za-z_][->A-Za-z_0-9]*\)\(([^0123456789]\)/\1\2/
s/\([A-Za-z_][->A-Za-z_0-9]*\)\(([^0123456789]\)/\1\2/
s/\([A-Za-z_][->A-Za-z_0-9]*\)\(([^0123456789]\)/\1\2/g
s/^{/@START/
s/^}/@STOP/
