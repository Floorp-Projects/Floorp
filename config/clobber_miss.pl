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

