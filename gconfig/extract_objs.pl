#! /usr/local/bin/perl
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
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

require('coreconf.pl');

$returncode =0;

&parse_argv;

open (INPUT, $var{LIST})  || die "Couldn't open input file $var{LIST}!\n";


while(<INPUT>)
{
  chop ;

  $obj = "/extract:".$_;

  $out = "/out:".$_;

  system 'lib', '/nologo', $obj, $out, $var{LIBRARY};

}

close (INPUT);


exit($returncode);





