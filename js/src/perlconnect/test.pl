#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
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