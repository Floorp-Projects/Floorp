#!perl5
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#
# Searches the tree for unclobbered files
#   should be relatively cross platform
#

$start_dir = $ENV{"MOZ_SRC"};
@ignore_list = ("make.dep","manifest.mnw");

$missed = 0;

print "\n\nChecking for unclobbered files\n" .
          "------------------------------\n";

GoDir("ns");
GoDir("mozilla");

if( $missed ){
    die "\nError: $missed files or directories unclobbered\n";
}
else {
    print "No unclobbered files found\n";
}

sub GoDir {
    local($dir) = @_;
    local(%filelist,$iscvsdir);
    local($k,$v,$d,$fn,$rev, $mod_time);
    local($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
                        $atime,$mtime,$ctime,$blksize,$blocks);

    if(! chdir "$start_dir/$dir" ){
        return;
    }

    while(<*.*> ){
        if( $_ ne '.' && $_ ne '..' && $_ ne 'CVS'
                && $_ ne 'nuke' ){
            $filelist{$_} = 1;
        }
    }

    if( -r "CVS/Entries" ){
        $iscvsdir=1;
        open(ENT, "CVS/Entries" ) || 
                die "Cannot open CVS/Entries for reading\n";
        while(<ENT>){
            chop;
            ($d,$fn,$rev,$mod_time) = split(/\//);

            if( $fn ne "" ){
                if( $d eq "D" ){
                    $filelist{$fn} = 3;
                }
                else {
                    $filelist{$fn} = 2;
                }
            }                                    
        }
        close(ENT);
    }

    while( ($k,$v) = each %filelist ){
        if( $v == 1 && $iscvsdir && !IgnoreFile( $k ) ){
            if( ! -d $k ){
                print "     file: $dir/$k\n";
                $missed++;
            }
            else {
                if( ! -r "$k/CVS/Entries" ){
                    print "directory: $dir/$k\n";
                    $missed++;
                }
                else {
                    $filelist{$k} = 3;
                }

            }
        }
    }

    while( ($k,$v) = each %filelist ){
        if( $v == 3 ){
            GoDir("$dir/$k");
        }
    }

#    while( ($k,$v) = each %filelist ){
#        print "$k: $v\n";
#    }

}

sub IgnoreFile {
    local($fn) = @_;
    local($i);

    for $i (@ignore_list){
        if( $fn eq $i ){
            return 1;
        }
    }
    return 0;
}

