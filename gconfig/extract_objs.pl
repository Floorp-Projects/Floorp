#! /usr/local/bin/perl
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

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





