#!/usr/bin/perl
# -*- Mode: Perl; tab-width: 4; indent-tabs-mode: nil; -*-

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla JavaScript Testing Utilities
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2007
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary <bclary@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# make stderr, stdout unbuffered

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $regchars = '\[\^\-\]\|\{\}\?\*\+\.\<\>\$\(\)';

sub escape_patterns;
sub unescape_patterns;
sub debug;

my $debug = $ENV{DEBUG};
my @outputlines = ();
my @inputlines = ();

while (<ARGV>) {
    chomp;

    # remove irrelevant data the caller is required to remove any
    # other data which should not be considered during the
    # consolidation such as TEST_MACHINE, etc.

    s/TEST_DATE=[^,]*,/TEST_DATE=.*,/;

    push @inputlines, ($_);

}

my @fieldnames = ('TEST_BRANCH', 'TEST_BUILDTYPE', 'TEST_TYPE', 'TEST_OS', 'TEST_PROCESSORTYPE', 'TEST_KERNEL', 'TEST_TIMEZONE');

my $pass = 0;
my $changed = 1;

while ($changed) {

    # repeated loop until no changes are made.

    ++$pass;
    $changed = 0;

    debug "pass $pass, " . ($#inputlines + 1) . " inputlines, " . ($#outputlines + 1) . " outputlines\n";

    foreach $field (@fieldnames) {

        debug "pass $pass, processing $field, " . ($#inputlines + 1) . " inputlines, " . ($#outputlines + 1) . " outputlines\n";

        # process each field across all lines so that later consolidations
        # will match consolidated field values

        while ($inputline = shift(@inputlines)) {

            debug "inputline $inputline\n";

            # get the current field value from the current input line

            ($inputvalue) = $inputline =~ /$field=\(?([^,\)]*)\)?,/;

            if ($inputvalue eq '.*') {

                # if the current input value is the any wildcard,
                # then there is no need to perform a consolidation
                # on the field.

                push @outputlines, ($inputline);

                next;
            }

            # turn "off" any regular expression characters in the input line

            $pattern = escape_pattern($inputline);

            # Make the current field in the current pattern an any
            # wildcard so that it will match any value. We are looking
            # for all other lines that only differ from the current line by
            # the current field value 

            $pattern =~ s/$field=[^,]*,/$field=.*,/;

            # find the matches to the current pattern

            debug "pattern: $pattern\n";

            @matched   = grep /$pattern/, (@inputlines, @outputlines);
            @unmatched = grep !/$pattern/, @inputlines;

            debug "" . ($#matched + 1) . " matched, " . ($#unmatched + 1) . " unmatched, " . ($#inputlines + 1) . " inputlines, " . ($#outputlines + 1) . " outputlines\n";

            if (@matched) {

                # the input line matched others

                $outputvalue = $inputvalue;

                foreach $matchline (@matched) {

                    ($matchvalue) = $matchline =~ /$field=\(?([^,\)]*)\)?,/;

                    if ( $inputvalue !~ /$matchvalue/ && $matchvalue !~ /$inputvalue/) {

                        # the current match value and input value
                        # do not overlap so add the match
                        # field value as regular expression
                        # alternation | to the current field value 

                        debug "adding regexp alternation to $field: inputvalue: $inputvalue, matchvalue: $matchvalue";

                        $outputvalue .= "|$matchvalue";
                    }
                } # foreach matchline

                # replace the current inputs field value with the
                # consolidated value

                if ($outputvalue =~ /\|/) {
                    $outputvalue = "(" . join('|', sort(split(/\|/, $outputvalue))) . ")";
                }
                $inputline =~ s/$field=[^,]*,/$field=$outputvalue,/;
                debug "$inputline\n";

                $changes = 1;
            }
            push @outputlines, ($inputline);

            @inputlines = @unmatched;

        } # while inputline
        
        @inputlines = @outputlines;
        @outputlines = ();

    } # foreach field
}

@inputlines = sort @inputlines;

my $output = join"\n", @inputlines;

debug "output: " . ($#inputlines + 1) . " lines\n";

print "$output\n";

### # look for over specified failures
### 
### $field = 'TEST_DESCRIPTION';
### 
### while ($inputline = shift(@inputlines)) {
### 
###     debug "inputline $inputline\n";
### 
###     # turn "off" any regular expression characters in the input line
### 
###     $pattern = escape_pattern($inputline);
### 
###     # Make the TEST_DESCRIPTION field in the current pattern an any
###     # wildcard so that it will match any value. We are looking
###     # for all other lines that only differ from the current line by
###     # the TEST_DESCRIPTION. These will be the potentially overspecified 
###     # failures.
### 
###     $pattern =~ s/$field=[^,]*,/$field=.*,/;
### 
###     # find the matches to the current pattern
### 
###     debug "pattern: $pattern\n";
### 
###     @matched   = grep /$pattern/, @inputlines;
###     @unmatched = grep !/$pattern/, @inputlines;
### 
###     debug "" . ($#matched + 1) . " matched, " . ($#unmatched + 1) . " unmatched, " . ($#inputlines + 1) . " inputlines, " . ($#outputlines + 1) . " outputlines\n";
### 
###     if (@matched) {
### 
###         # the inputline overspecifies an error
### 
###         push @matched, ($inputline);
### 
###         foreach $matchline (@matched) {
### 
###             print STDERR "OVERSPECIFIED? : $matchline\n";
### 
###         } # foreach matchline
### 
###     }
### 
###     @inputlines = @unmatched;
### 
### } # while inputline
### 



sub escape_pattern {

    # unlike the known-failures.pl, this escape escapes the entire 
    # line to make it not contain any active regular expression patterns
    # so that any matched will be literal and not regular
    my $line = shift;

    chomp;

    # replace unescaped regular expression characters in the 
    # description so they are not interpreted as regexp chars
    # when matching descriptions. leave the escaped regexp chars
    # `regexp alone so they can be unescaped later and used in 
    # pattern matching.

    # see perldoc perlre

    $line =~ s/\\/\\\\/g;

    # escape regexpchars
    $line =~ s/([$regchars])/\\$1/g;

    return "$line";

}

sub debug {
    my $msg;
    if ($debug) {
        $msg = shift;
        print "DEBUG: $msg\n";
    }
}
