#
# replace.pl: perform simple string substitution on a file
# the first line in the input (template) file is also discarded.
#
# usage: perl replace.pl KEYWORD=VALUE... < TMPLFILE > OUTFILE
#
# created 17 October 2001 by Mark Smith <mcs@netscape.com>

use File::Basename;
push @INC, dirname($0);

require replace;

%keywords = {};

foreach $str (@ARGV) {
	($key,$val) = split( "=", $str, 2 );
	$keywords{$key} = $val;
}

replace::GenerateHeader(*STDIN, *STDOUT, \%keywords);

exit 0;
