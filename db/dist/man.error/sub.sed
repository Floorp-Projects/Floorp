# @(#)sub	8.3 (Sleepycat) 3/22/97
# Build of set of sed commands to do substitution on the names.

H
/    !STOP/{
	x
	/^_/{
		y/\n/ /
		s;  *; ;g
		s; $;;
		s; !START;$/;
		s; !STOP;/;
		s; !;\\    !;g
		s;^;s/    !;
		p
	}
	n
	h
}
d
