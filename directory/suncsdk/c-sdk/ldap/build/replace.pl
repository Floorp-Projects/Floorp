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
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
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

exit 0;
