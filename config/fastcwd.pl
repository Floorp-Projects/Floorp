#!perl5
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
# By John Bazik
#
# Usage: $cwd = &fastcwd;
#
# This is a faster version of getcwd.  It's also more dangerous because
# you might chdir out of a directory that you can't chdir back into.
# 

sub fastcwd {
	local($odev, $oino, $cdev, $cino, $tdev, $tino);
	local(@path, $path);
	local(*DIR);

	($cdev, $cino) = stat('.');
	for (;;) {
		($odev, $oino) = ($cdev, $cino);
		chdir('..');
		($cdev, $cino) = stat('.');
		last if $odev == $cdev && $oino == $cino;
		opendir(DIR, '.');
		for (;;) {
			$_ = readdir(DIR);
			next if $_ eq '.';
			next if $_ eq '..';

			last unless $_;
			($tdev, $tino) = lstat($_);
			last unless $tdev != $odev || $tino != $oino;
		}
		closedir(DIR);
		unshift(@path, $_);
	}
	chdir($path = '/' . join('/', @path));
	$path;
}
1;
