#!/usr/bin/perl
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is JavaScript Core Tests.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1997-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Robert Ginda
#   Bob Clary
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# Creates the meat of a test suite manager page, requites menuhead.html and menufoot.html
# to create the complete page.  The test suite manager lets you choose a subset of tests
# to run under the runtests2.pl script.

local $lxr_url = "http://lxr.mozilla.org/mozilla/source/js/tests/";
local $suite_path = $ARGV[0] || "./";
local $uid = 0;          # radio button unique ID
local $html = "";        # html output
local $javascript = "";  # script output

&main;

print (&scriptTag($javascript) . "\n");
print ($html);

sub main {
    local $i, @suite_list;

    if (!($suite_path =~ /\/$/)) {
	$suite_path = $suite_path . "/";
    }

    @suite_list = sort(&get_subdirs ($suite_path));

    $javascript .= "\n";
    $javascript .= "var suites = {};\n";
    $javascript .= "function populateSuites()\n";
    $javascript .= "{\n";
    $javascript .= "var currSuite;\n";
    $javascript .= "var currDirectory;\n";
    $javascript .= "var currTestDirs;\n";
    $javascript .= "var currTests;\n";

    $html .= "<h3>Test Suites:</h3>\n";
    $html .= "<center>\n";
    $html .= "<input type='button' value='Select All' " .
      "onclick='selectAll();'> \n";
    $html .= "<input type='button' value='Select None' " .
      "onclick='selectNone();'> \n";

    # suite menu
    $html .= "<table border='1' summary='suite menu'>\n";
    foreach $suite (@suite_list) {
	local @readme_text = ("No description available.");
	if (open (README, $suite_path . $suite . "/README")) {
	    @readme_text = <README>;
	    close (README);
	}
	$html .= "<tr>\n";
	$html .= "<td>\n";
	$html .= "<a href='\#SUITE_$suite'>$suite</a>\n";
	$html .= "</td>\n";
	$html .= "<td>@readme_text</td>\n";
	$html .= "<td>\n";
	$html .= "<input type='button' value='Select All' " .
	  "onclick='selectAll(\"$suite\");'> \n";
	$html .= "<input type='button' value='Select None' " .
	  "onclick='selectNone(\"$suite\");'>\n";
	$html .= "</td>\n";
	$html .= "<td><input readonly name='SUMMARY_$suite'></td>";
	$html .= "</tr>";
    }
    $html .= "</table>\n";
    $html .= "<div><input readonly name='TOTAL'></div>\n";
    $html .= "</center>\n";

    $html .= "<dl>\n";
    foreach $i (0 .. $#suite_list) {
	local $prev_href = ($i > 0) ? "\#SUITE_" . $suite_list[$i - 1] : "";
	local $next_href = ($i < $#suite_list) ? "\#SUITE_" . 
	    $suite_list[$i + 1] : "";
	&process_suite ($suite_path, $suite_list[$i], $prev_href, $next_href);
    }
    $html .= "</dl>\n";

    $javascript .= "}\n";
    $javascript .= "populateSuites();\n";
}

#
# Append detail from a 'suite' directory (eg: ecma, ecma_2, js1_1, etc.), calling
# process_test_dir for subordinate categories.
#
sub process_suite {
    local ($suite_path, $suite, $prev_href, $next_href) = @_;
    local $i, @test_dir_list;    
    local $suite_count = 0;

    # suite js object
    $javascript .= "currSuite = suites[\"$suite\"] = " .
	"{testDirs:{}, count:0, selected:0};\n";
    $javascript .= "currTestDirs = currSuite.testDirs;\n";

    @test_dir_list = sort(&get_subdirs ($test_home . $suite));

    # suite header

    $html .= "<dt>\n";
    $html .= "<a name='SUITE_$suite'></a>$suite " .
      "(" . ($#test_dir_list + 1) . " Sub-Categories)\n";
    $html .= "<input type='button' value='Select All' " .
	"onclick='selectAll(\"$suite\");'>\n";
    $html .= "<input type='button' value='Select None' " .
	"onclick='selectNone(\"$suite\");'> \n";
    $html .= "[ ";
    $html .= "<a href='\#top_of_page'>Top of page</a> \n";
    if ($prev_href) {
	$html .= " | ";
        $html .= "<a href='$prev_href'>Previous Suite</a> \n";
    }
    if ($next_href) {
	$html .= " | ";
        $html .= "<a href='$next_href'>Next Suite</a> \n";
    }
    $html .= "]\n";
    $html .= "</dt>\n";
    $html .= "<dd>\n";

    foreach $i (0 .. $#test_dir_list) {
	local $prev_href = ($i > 0) ? "\#TESTDIR_" . $suite . 
	    $test_dir_list[$i - 1] :
	    "";
	local $next_href = ($i < $#test_dir_list) ?
	  "\#TESTDIR_" . $suite . $test_dir_list[$i + 1] : "";
	&process_test_dir ($suite_path . $suite . "/", $test_dir_list[$i], 
			   $suite, 
			   $prev_href, $next_href);
    }
    $javascript .= "currSuite.count = $suite_count;\n";

    $html .= "</dd>\n";
}

#
# Append detail from a test directory, 
# calling process_test for subordinate js files
#
sub process_test_dir {
    local ($test_dir_path, $test_dir, $suite, $prev_href, $next_href) = @_;

    @test_list = sort(&get_js_files ($test_dir_path . $test_dir));

    $suite_count += @test_list;

    $javascript .= "currDirectory = currTestDirs[\"$test_dir\"] = " . 
	"{tests:{}};\n";
    $javascript .= "currTests = currDirectory.tests;\n";

    $html .= "<dl>\n";
    $html .= "<dt>\n";
    $html .= "<a name='TESTDIR_$suite$test_dir'></a>\n";
    $html .= "$test_dir (" . ($#test_list + 1) . " tests)\n";
    $html .= "<input type='button' value='Select All' " .
	"onclick='selectAll(\"$suite\", \"$test_dir\");'>\n";
    $html .= "<input type='button' value='Select None' " .
	"onclick='selectNone(\"$suite\", \"$test_dir\");'> ";
    $html .= "[ <a href='\#SUITE_$suite'>Top of $suite Suite</a> ";
    if ($prev_href) {
	$html .= "| <a href='$prev_href'>Previous Category</a> ";
    }
    if ($next_href) {
	$html .= " | <a href='$next_href'>Next Category</a> ";
    }
    $html .= "]\n";
    $html .= "</dt>\n";

    $html .= "<dd>\n";

    foreach $test (@test_list) {
	&process_test ($test_dir_path . $test_dir, $test);
    }
    
    $html .= "</dl>\n";
}


#
# Append detail from a single JavaScript file.
#
sub process_test {
    local ($test_dir_path, $test) = @_;
    local $title = "";

    $uid++;

    open (TESTCASE, $test_dir_path . "/" . $test) ||
      die ("Error opening " . $test_dir_path . "/" . $test);

    while (<TESTCASE>) {
	if (/.*(TITLE|summary)\s+\=\s+[\"\'](.*)[\"\']/i) {
	    $title = $2;
	    break;
	}
	if (/.*START\([\"\'](.*)[\"\']\);/)
        {
	    $title = $1;
	    break;
	}
    }
    close (TESTCASE);

    $javascript .= "currTests[\"$test\"] = {id:\"t$uid\"}\n";
    $html .= "<dd>\n";
    $html .= " <input type='checkbox' value='$test' name='t$uid' " .
        "id='$suite:$test_dir:$test' ".
	"onclick='return onRadioClick(this);'>\n";
    $html .= "<a href='$lxr_url$suite/$test_dir/$test' target='other_window'>" .
	"$test</a> $title\n";
    $html .= "</dd>\n";

}

sub scriptTag {

    return ("<script type='text/javascript' language='JavaScript'>@_</script>");

}

#
# given a directory, return an array of all subdirectories
#
sub get_subdirs {
    local ($dir)  = @_;
    local @subdirs;

    if (!($dir =~ /\/$/)) {
	$dir = $dir . "/";
    }

    opendir (DIR, $dir) || die ("couldn't open directory $dir: $!");
    local @testdir_contents = readdir(DIR);
    closedir(DIR);

    foreach (@testdir_contents) {
        if ((-d ($dir . $_)) && ($_ ne 'CVS') && ($_ ne '.') && ($_ ne '..')) {
            @subdirs[$#subdirs + 1] = $_;
        }
    }

    return @subdirs;
}

#
# given a directory, return an array of all the js files that are in it.
#
sub get_js_files {
    local ($test_subdir) = @_;
    local @js_file_array;

    opendir ( TEST_SUBDIR, $test_subdir) || die ("couldn't open directory " .
						 "$test_subdir: $!");
    @subdir_files = readdir( TEST_SUBDIR );
    closedir( TEST_SUBDIR );

    foreach ( @subdir_files ) {
        if ( ($_ =~ /\.js$/) && ($_ ne 'shell.js') && ($_ ne 'browser.js') ) {
            $js_file_array[$#js_file_array+1] = $_;
        }
    }

    return @js_file_array;
}


