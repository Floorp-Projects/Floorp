#!perl -w
package replace;

require 5.004;

use strict;
require Exporter;

use vars qw(@ISA @EXPORT);

@ISA      = qw(Exporter);
@EXPORT   = qw(
                GenerateHeader
              );

sub GenerateHeader ($$\%) {
    my($template, $header, $keywords) = @_;
    local(*TEMPLATE) = $template;
    local(*HEADER) = $header;

    my($first_line) = 1;
    my($orig, $replace);

    while(<TEMPLATE>) {
        my $line = $_;
        while(($orig, $replace) = each %$keywords) {
            $line =~ s/{{$orig}}/$replace/g;
        }
	
	# the first line is a comment specific to the template file, which we
	# don't want to transfer over
	#
        if ( ! $first_line ) {
            print HEADER $line;
        } else {
            $first_line = 0;
        }
    }
}
