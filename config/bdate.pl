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

# XP way of doing the build date.
# 1998091509 = 1998, September, 15th, 9am local time zone
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime;
# localtime returns year minus 1900
$year = $year + 1900;
printf("%04d%02d%02d%02d\n", $year, 1+$mon, $mday, $hour);
