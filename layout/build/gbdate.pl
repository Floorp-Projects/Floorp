#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
