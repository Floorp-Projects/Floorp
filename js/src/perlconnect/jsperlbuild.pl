#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.

#
# Helper perl script to abstract the location of xsubpp
#

# Thanks to Dave Neuer <dneuer@futuristics.net> for the original
# version of this file.  Entered as NPL with his permission.

use File::Find;
use Cwd;

$notfound = 1;

foreach(@INC){
	next if /^\.$/;
	if(-e $_){
		find(\&find_ExtUtils, $_);
	}
}

if(-e $xsubpp && -e $typemap){
	$res = `perl $xsubpp -typemap $typemap -typemap typemap JS.xs > JS.c`;
	if(-e "JS.c"){
	        print "Successfuly built JS.c\n";
		exit(0);
	}
	else{
		die("Couldn't build JS.c: $res");
	}
}
else{
	die("Couldn't locate files neccessary for building JS.c");
}

sub find_ExtUtils{
	if($notfound){
		if($File::Find::dir =~ /ExtUtils/){	
			my $path = cwd;
			$xsubpp = $path . "/xsubpp";
			$typemap = $path . "/typemap";
			undef $notfound;
		}
	}
}
