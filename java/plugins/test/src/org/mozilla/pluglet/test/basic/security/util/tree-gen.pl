#!/usr/local/bin/perl
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s):
#
# tree-gen.pl
#
#	Generating the tree of subfolders with
#	properties file.
#

#Cwd used to determine top dir.
use Cwd 'abs_path';
#

$depth = $ARGV[0];
printf "depth=$depth\n";

if ($^O=~/Win32/) {
    $file_separator = "\\";
} else {
    $file_separator = "\/";
}

$sd_prefix  = "$depth/build/test/basic/security";
$class_list = "SecClassList.lst";
$std_fname  = "TestProperties";
$props_list = "$depth/config/SecurityTests.properties";
$pluglet    = "build/classes/org/mozilla/pluglet/test/basic/security/TestSecurityPluglet.jar";
$top_dir = abs_path($depth);
$abs_pluglet = $top_dir."/".$pluglet;
$abs_pluglet =~s#\/#$file_separator#g;
$base_html  = "security/bastest.html";
print $abs_pluglet,"\n";
$package_prefix = "org.mozilla.pluglet.test.basic.security";

# note, that we will generate all the folders directly from
# this app.

open( lst, $class_list ) or die("Can't open class list file\n");

if( !(-e $sd_prefix) ) {
	mkdir($sd_prefix, 0777);
};
	
if(!( -e  "$sd_prefix/automation") ) {
	$t = join '', $sd_prefix, "/automation";
	mkdir("$sd_prefix/automation", 0777);
};

# This cycle implements the properties extraction file
while( $l = <lst> ) {
	$tgt_prefix=$sd_prefix;
	chop $l; # Avoid linebreaks
	$class_name=$l;
	$pkg_prefix = $package_prefix;
	@temp = split /\./,$class_name;
	$pure_class = $temp[$#temp];
	    $pure_class =~s/\r|\n//g;
	
	if( $l =~ "automation" ) {
		$tgt_prefix = join '',$sd_prefix,"/automation";
		$pkg_prefix = join '',$package_prefix,".automation";
	};

	$class_to_call = "$pkg_prefix.$pure_class";
	$folder_to_create = "$tgt_prefix/$pure_class";
	if( !(-e $folder_to_create) ) {
		mkdir($folder_to_create,0777);
		printf "$folder_to_create\n";
	};
	
	open( out, ">$folder_to_create/TestProperties" ) or 
		die("Can't create file $folder_to_create/TestProperties");

	open( in, $props_list ) 
		or die(".properties file was not found.\n");

	# Extract all the properties, we need
		
	while( $ln = <in> ) {
		if( $ln =~ /$pure_class/ ) {
			printf out $ln;
		};
	};
	printf out "RUN_STAGES=PLUGLET_INITIALIZE\n";
	printf out "TEST_CLASS=$class_to_call\n";
	printf out "TEST_HTML=$base_html\n";
	printf out "PLUGLET=$abs_pluglet\n";

	close in;
};
