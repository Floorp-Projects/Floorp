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
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s):
#  Norris Boyd
# 

print "Running...\n";

open(IN, "nsDOMPropEnums.h") || 
	die("Error opening 'nsDOMPropEnums.h': $!\n");

open(OUT, ">nsDOMPropNames.h") ||
	die("Error opening 'nsDOMPropNames.h': $!\n");

while (<IN>) {
	if (/^enum nsDOMProp/) {
		last;
	}
}

print OUT <<'EOF';
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 */

/* nsDOMPropNames.h -- an definition of all DOM property names used to provide 
**                     per-property security policies.
**                     AUTOMATICALLY GENERATED -- See genPropNames.pl 
*/

#ifndef nsDOMPropNames_h__
#define nsDOMPropNames_h__

#define NS_DOM_PROP_NAMES \
EOF

$last = "";

while (<IN>) {
	if (/NS_DOM_PROP_MAX/) {
		last;
	}
	$save = $_;
	s/,.*/", \\/;
	s/NS_DOM_PROP_/"/;
	s/_/./;
	$_ = lc($_);
	print OUT $_;
	# Check order of names and make sure they are sorted.
	# It's important we check after the subsitution of '.' for '_'
	# since it's the sort order of the names we care about and '.'
	# and '_' sort differently with respect to letters.
	if ($last ne "" && ($last gt $_)) {
		die "Name $lastsave and $save are out of order in nsDOMPropEnums.h.\n";
	}
	$last = $_;
	$lastsave = $save;
}

print OUT <<EOF;

#endif // nsDOMPropNames_h__
EOF

print "Done\n";
