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
# configurator.pl - properties configuration utility
#	
# see configurator.readme for details
#

# Global variables

$infile   = @ARGV[0];
$outfile  = @ARGV[1];
$template = @ARGV[2];

if( $#ARGV < 2) {
	printf "Usage: perl configurator.pl infile outfile template\n";
	printf "See configurator.readme for details.\n";
	exit 0;
};

open( in, $infile ) 
	or die( join '', "Can't open", $infile, " for reading. So bye\n" );
open( out, join '',">",$outfile ) 
	or die( join '', "Can't open", $outfile, " for writing. So bye\n" );
open( temple, $template ) 
	or die( join '', "Can't open template file ",$template," for reading. So bye.\n" );

# Now we will read out all the template file lines into the array.
@storage = <temple>;

while( $l = <in> ) {
# Sign '=' was found and a line isn't a comment
	if( ($l !=~ /#/) && ($l =~ /=/) ) {
		chop $l; 
	        $l=~s/\r|\n//g;
# avoid linebreaks
		@list = split '=',$l;
		#  Now opening template file

		$keyvalue = @list[1];
		$keyname = @list[0];

		# Now search for keyname and replace it with the actual value.
		$pattern = join '', "<",$keyname,">";

		for( $i=0; $i<= $#storage; $i++ ) {

			$line = @storage[$i];
			if( $line =~ /$pattern/ ) {
			       $line=~s/$pattern/$keyvalue/g;
			       $storage[$i] = $line;
			} # if
		} # for
	} # if
} # while

# end-of-search
# preparing the output file

for( $i=0; $i <= $#storage; $i++ ) {
	
	printf out $storage[$i];
}

# job was done
