#! perl
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
#

require "fastcwd.pl";

$_ = &fastcwd;
if (m@^/[uh]/@o || s@^/tmp_mnt/@/@o) {
    print("$_\n");
} elsif ((($user, $rest) = m@^/usr/people/(\w+)/(.*)@o)
      && readlink("/u/$user") eq "/usr/people/$user") {
    print("/u/$user/$rest\n");
} else {
    chop($host = `hostname`);
    print("/h/$host$_\n");
}
