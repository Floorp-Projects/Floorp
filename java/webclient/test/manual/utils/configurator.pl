#!/bin/perl
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s):

###################################################################
#
# configurator.pl - configuration utility
#	


sub mkdir_p {
    my @dirs=split(/$file_separator/,shift(@_));
    my $curdir="";
    for($i=0;$i<$#dirs;$i++) {
	$curdir.=$dirs[$i].$file_separator;
	mkdir($curdir,0775);
    }
}

if( $#ARGV != 2) {
	printf "Usage: perl configurator.pl infile outfile template\n";
	exit 0;
};

# Global variables
$infile   = $ARGV[0];
$outfile  = $ARGV[1];
$template = $ARGV[2];
$infile =~ s#\\#/#g;
$outfile =~s#\\#/#g;
$template=~s#\\#/#g;
 
$file_separator = "\/";



open( IN, $infile ) || die("Can't open $infile for reading. $!\n");
open( TEMPLATE, $template ) || die ("Can't open template file $template for reading. $!\n");

# Now we will read out all the template file lines into the array.
@storage = <TEMPLATE>;

while( $l = <IN> ) {
	if( ($l !=~ /$\s*\#/) && ($l =~ /=/) ) {
	        $l=~s/\r|\n|\s//g;
		($keyname,$keyvalue) = split('=',$l);
		$pattern = "<".$keyname.">";
		for( $i=0; $i<= $#storage; $i++ ) {
			$line = $storage[$i];
			if( $line =~ /$pattern/ ) {
			       $line=~s/$pattern/$keyvalue/g;
			       $storage[$i] = $line;
			} # if
		} # for
	} # if
} # while

close(TEMPLATE);
close(IN);

# end-of-search
# preparing the output file
#Create necessary directories
mkdir_p($outfile);
open( OUT, ">".$outfile) || die("Can't open $outfile for writing. $!\n");

for( $i=0; $i <= $#storage; $i++ ) {
     print OUT $storage[$i];
}

close(OUT);




