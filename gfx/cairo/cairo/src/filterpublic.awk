#!/bin/awk -f
# Reads cairo header files on stdin, and outputs a file with defines for
# renaming all public functions to Mozilla-specific names.
# Usage:
#   cat *.h | ./filterpublic.awk | sort > cairo-rename.h

BEGIN { state = "public"; }

/^cairo_public/ { state = "function"; next; }
/[a-zA-Z_]+/	{
			if (state == "function") {
				print "#define " $1 " _moz_" $1;
				state = "public";
			}
		}

# catch some one-off things
END {
}
