#
# replace.pl: perform simple string substitution on a file
# the first line in the input (template) file is also discarded.
#
# usage: perl replace.pl KEYWORD=VALUE... < TMPLFILE > OUTFILE
#
# created 17 October 2001 by Mark Smith <mcs@netscape.com>

@keywords = ();
@values = ();

$count = 0;
foreach $str (@ARGV) {
	($key,$val) = split( "=", $str, 2 );
	push (@keywords, $key);
	push (@values, $val);
	++$count;
}

$first_line = 1;

while(<STDIN>) {
	$line = $_;
	$count = 0;
	foreach $str (@keywords) {
		$line =~ s/{{$str}}/$values[$count]/g;
		++$count;
	}

	if ( ! $first_line ) {
		print $line;
	} else {
		$first_line = 0;
	}
}

#exit 0;
