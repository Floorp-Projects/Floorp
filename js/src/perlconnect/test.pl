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
#

#
# Test file for JS.pm
#

use JS;
# create new JS context
($js = new JS()) or warn "new JS() failed";
# eval for simple scalar cases
# int
($js->eval("100") == 100) or warn "Wrong value returned from eval()";
# and string
($js->eval("'test string'") eq 'test string') or warn "Wrong value returned from eval()";
# double TODO: double comparison?
($js->eval("1.25") == 1.25) or warn "Wrong value returned from eval()";
# more complex eval:
# return an object
$obj = $js->eval(q/
    x = new Object();
    x.t='a';
    x;
/) or warn "eval failed";
# tie this object to a hash
tie %hash, "JS::Object", $obj;
# try retrieving and manipulating values
$hash{foo} = 1200;
$_ = $hash{foo};
($_ == 1200) or warn "Wrong value returned from hash";
#
$hash{bar} = 'abcdef';
$_ = $hash{bar};
($_ eq 'abcdef') or warn "Wrong value returned from hash";
# exists/delete
(exists $hash{bar}) or warn "\$hash{bar} should exist";
delete $hash{bar};
(!exists $hash{bar}) or warn "\$hash{bar} should be deleted";
# exists/clear
(exists $hash{foo}) or warn "\$hash{foo} should exist";
undef %hash;
(!exists $hash{foo}) or warn "\$hash{foo} should be deleted";