#!<PERL_DIR>/perl
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
# This script is used to reinforced testing of Webclient API.
# It invokes autorun.pl script a several times and then compare
# results. 
# Webclient M4 isn't stable under Solaris and this tool can be used
# to get more truthfull results


##################################################################
# Usage
##################################################################
sub usage() {

    print "\n";
    print "##################################################################\n";
    print " Usage: perl forceful.pl n -o <output_file> <autorun args> \n";
    print "\n";
    print " where n is number between 2 and 9 (2 <= n <= 9)\n\n";
    print " <output_file> is file for writing comarison table\n\n";
    print " and <autorun args> are valid autorun.pl script arguments.\n";
    print "\n";
    print "##################################################################\n";
    print "\n";
}

##################################################################
# Main
##################################################################

unless (($#ARGV !=-1)&&($ARGV[0]=~/[2-9]/)&&($ARGV[1] eq "-o")) {
    usage();
    exit -1;
}
# Declare
$DEPTH="../..";
$repeat="";
$autorun_args="";
%logs;




$repeat=shift @ARGV;
$output_file = shift @ARGV; #avoid -o switch
$output_file = shift @ARGV; #avoid -o switch
$autorun_args = join(" ",@ARGV);
%logs=split(/,/,join(",1,",<$DEPTH/log/*>).",1");
while($repeat>0) {
    print "$autorun_args \n";
    system("perl autorun.pl $autorun_args"); 
    $repeat--;
}
%new_logs=split(/,/,join(",1,",<$DEPTH/log/*>).",1");
$logcount=0;
foreach $logdir (keys %new_logs) {
    unless(defined($logs{$logdir})) {
	print "Unique: $logdir -------- $logs{$logdir}\n";
	if(-f "$logdir/BWTest.txt") {
	    $logfiles[$logcount] = "$logdir/BWTest.txt";
	    $logcount++;
	}
    }
}
if($#logfiles<=0) {
    print "Error. Aren't enouth logs for creating diff\n";
    exit -1;
}

print "Execute perl $DEPTH/utils/compare.pl -b -o $output_file ".join(" ",@logfiles);
system("perl $DEPTH/utils/compare.pl -b -o $output_file ".join(" ",@logfiles));










