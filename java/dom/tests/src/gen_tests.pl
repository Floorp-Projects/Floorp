#!/usr/bin/perl
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

($#ARGV > 0) || die "Usage: gen_rules.pl <file_list> <output_dir> <rules_file>\n";

open (IN, "<$ARGV[0]")  || die "Can't open in  file $ARGV[0]: $!\n";

while(<IN>) {
	if (/.*\d (.*?)\.java/) {
		$elem = $1;
		$name = "UNKNOWN_ELEMENT";
		if ($elem =~ /HTML(.*)Element/) {
			$name = $1;
		}
		system "build_test.bat -d $ARGV[1] -r $ARGV[2] org.w3c.dom.html.$elem";
	}

}
