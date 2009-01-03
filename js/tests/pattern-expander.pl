#!/usr/bin/perl -w
# -*- Mode: Perl; tab-width: 4; indent-tabs-mode: nil; -*-
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
# The Original Code is Mozilla JavaScript Testing Utilities
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary <bclary@bclary.com>
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

# usage: pattern-expander.pl knownfailures > knownfailures.expanded
#
# pattern-expander.pl reads the specified knownfailures file and
# writes to stdout an expanded set of failures where the wildcards
# ".*" are replaced with the set of possible values specified in the
# universe.data file.

use lib $ENV{TEST_DIR} . "/tests/mozilla.org/js";

use Patterns;

package Patterns;

processfile();

sub processfile
{
    my ($i, $j);

    while (<ARGV>) {

        chomp;

        $record = {};

        my ($test_id, $test_branch, $test_repo, $test_buildtype, $test_type, $test_os, $test_kernel, $test_processortype, $test_memory, $test_cpuspeed, $test_timezone, $test_result, $test_exitstatus, $test_description) = $_ =~ 
            /TEST_ID=([^,]*), TEST_BRANCH=([^,]*), TEST_REPO=([^,]*), TEST_BUILDTYPE=([^,]*), TEST_TYPE=([^,]*), TEST_OS=([^,]*), TEST_KERNEL=([^,]*), TEST_PROCESSORTYPE=([^,]*), TEST_MEMORY=([^,]*), TEST_CPUSPEED=([^,]*), TEST_TIMEZONE=([^,]*), TEST_RESULT=([^,]*), TEST_EXITSTATUS=([^,]*), TEST_DESCRIPTION=(.*)/;

        $record->{TEST_ID}            = $test_id;
        $record->{TEST_BRANCH}        = $test_branch;
        $record->{TEST_REPO}          = $test_repo;
        $record->{TEST_BUILDTYPE}     = $test_buildtype;
        $record->{TEST_TYPE}          = $test_type;
        $record->{TEST_OS}            = $test_os;
        $record->{TEST_KERNEL}        = $test_kernel;
        $record->{TEST_PROCESSORTYPE} = $test_processortype;
        $record->{TEST_MEMORY}        = $test_memory;
        $record->{TEST_CPUSPEED}      = $test_cpuspeed;
        $record->{TEST_TIMEZONE}      = $test_timezone;
        $record->{TEST_RESULT}        = $test_result;
        $record->{TEST_EXITSTATUS}    = $test_exitstatus;
        $record->{TEST_DESCRIPTION}   = $test_description;

        dbg("processfile: \$_=$_");

        my @list1 = ();
        my @list2 = ();

        my $iuniversefield;
        my $universefield;

        $item1 = copyreference($record);
        dbg("processfile: check copyreference");
        dbg("processfile: \$record=" . recordtostring($record));
        dbg("processfile: \$item1=" . recordtostring($item1));

        push @list1, ($item1);

        for ($iuniversefield = 0; $iuniversefield < @universefields; $iuniversefield++)
        {
            $universefield = $universefields[$iuniversefield];

            dbg("processfile: \$universefields[$iuniversefield]=$universefield, \$record->{$universefield}=$record->{$universefield}");

            for ($j = 0; $j < @list1; $j++)
            {
                $item1 = $list1[$j];
                dbg("processfile: item1 \$list1[$j]=" . recordtostring($item1));
                # create a reference to a copy of the hash referenced by $item1
                if ($item1->{$universefield} ne '.*')
                {
                    dbg("processfile: literal value");
                    $item2 = copyreference($item1);
                    dbg("processfile: check copyreference");
                    dbg("processfile: \$item1=" . recordtostring($item1));
                    dbg("processfile: \$item2=" . recordtostring($item2));
                    dbg("processfile: pushing existing record to list 2: " . recordtostring($item2));
                    push @list2, ($item2);
                }
                else
                {
                    dbg("processfile: wildcard value");
                    $keyfielduniversekey = getuniversekey($item1, $universefield);
                    @keyfielduniverse = getuniverse($keyfielduniversekey, $universefield);

                    dbg("processfile: \$keyfielduniversekey=$keyfielduniversekey, \@keyfielduniverse=" . join(',', @keyfielduniverse));

                    for ($i = 0; $i < @keyfielduniverse; $i++)
                    {
                        $item2 = copyreference($item1);
                        dbg("processfile: check copyreference");
                        dbg("processfile: \$item1=" . recordtostring($item1));
                        dbg("processfile: \$item2=" . recordtostring($item2));
                        $item2->{$universefield} = $keyfielduniverse[$i];
                        dbg("processfile: pushing new record to list 2 " . recordtostring($item2));
                        push @list2, ($item2);
                    }
                }
                for ($i = 0; $i < @list1; $i++)
                {
                    dbg("processfile: \$list1[$i]=" . recordtostring($list1[$i]));
                }
                for ($i = 0; $i < @list2; $i++)
                {
                    dbg("processfile: \$list2[$i]=" . recordtostring($list2[$i]));
                }
            }

            @list1 = @list2;
            @list2 = ();
        }
        for ($j = 0; $j < @list1; $j++)
        {
            $item1 = $list1[$j];
            push @records, ($item1);
        }
    }
    @records = sort sortrecords @records;

    dumprecords();
}

