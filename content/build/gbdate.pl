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

$given_date = $ENV{"MOZ_BUILD_DATE"};

if ($given_date ne "") {
	# The gecko date does not use hour resolution
	chop($given_date);
	chop($given_date);
	printf("#define PRODUCT_VERSION \"%08d\"\n", $given_date);
	exit(0);
}

# XP way of doing the build date.
# 1998091509 = 1998, September, 15th, 9am local time zone
($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime;
# localtime returns year minus 1900
$year = $year + 1900;
printf("#define PRODUCT_VERSION \"%04d%02d%02d\"\n", $year, 1+$mon, $mday);
