#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1999 Netscape Communications Corporation.  All Rights
# Reserved.

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
