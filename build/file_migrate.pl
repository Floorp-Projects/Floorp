#!perl -w

# This script copies modified files from a source CVS tree to a destination
# tree. Modified files are detected by comparing their modification dates
# with the CVS Entries file.
# 
# Modified files are copied in their entirety to the destination tree
# (no diffing is done). Files are only copied of the CVS version of the
# file is the same in both trees. If the destination file is modified
# already, it is backed up and replaced.
# 
# To use this on your tree/platform, do the following:
# 
# 1. Fix the !perl line, if necessary.
# 2. Fix $dirsep to be the directory separator on your platform.
# 3. Uncomment the appropriate $dst_linebreaks file specify what linebreaks
#    you want for the copied files. This variable defines the *destination* linebreaks 
#    that you want your changes to be converted to.
#    For example, if you have a linux volume 
#    mounted (via SAMBA perhaps) to your windows box where you've made changes to 
#    source files, you'd want $dst_linebreaks to be set for unix. This ensures that 
#    linebreaks are converted to the appropriate OS linebreak scheme for your *target* tree.
# 4. Set $src_tree and $dest_tree to point to the directories you want
#    to sync up. These don't have to point to the root of the tree,
#    but should be equivalent directories in the two trees.
#    
# First version:
#     Simon Fraser <sfraser@netscape.com>

use File::stat;
use Time::Local;

# change for your platform ('\' == windows, ':' == mac, '/' == unix)
$dirsep = ":";

# Set this to the native OS of the *destination* tree
# $dst_linebreaks = pack("cc", 13);           # Mac
$dst_linebreaks = pack("cc", 13, 10);       # DOS
# $dst_linebreaks = pack("cc", 10);           # UNIX

#change for your src and dest trees
$src_tree =  "Development:Mozilla dev:src:mozilla:editor";
$dest_tree = "src:mozilla:editor";


#//--------------------------------------------------------------------------------------------------
#// _copyFile. Copy file from src to dest, converting linebreaks if necessary
#//--------------------------------------------------------------------------------------------------
sub _copyFile($;$;$;$)
{
    my($srcdir, $destdir, $file, $backup) = @_;

    my($srcfile) = $srcdir.$dirsep.$file;
    my($dstfile) = $destdir.$dirsep.$file;
    
    if ($backup)
    {
        my($counter) = 0;
        
        while (-f $dstfile."-".$counter)
        {
            $counter ++;
        }
        
        rename($dstfile, $dstfile."-".$counter) or die "Failed to rename file\n";
    }
    
    print "Copying $file over to dest\n";

    my($newdest) = $dstfile."_temp";
    
    open(SRCFILE, "< $srcfile")     or die "Can't open source file $srcfile\n";
    open(NEWDEST, "> $newdest")     or die "Can't open dest file $newdest\n";
    
    while (<SRCFILE>)
    {
        chomp($_);
        print NEWDEST $_.$dst_linebreaks;
    }
    
    close(SRCFILE);
    close(NEWDEST);
    
    if (!$backup) {
        unlink($dstfile)  or die "Failed to remove $dstfile\n";
    }
    rename($newdest, $dstfile)      or die "Failed to rename $newdest\n";
}


#//--------------------------------------------------------------------------------------------------
#// _readCVSInfo. Suck in the CVS info from the Entries file
#//--------------------------------------------------------------------------------------------------

sub _readCVSInfo($)
{
    my($cvsDir) = @_;

    my($entries_file_name) = $cvsDir.$dirsep."CVS".$dirsep."Entries";
    
    # print "Reading $entries_file_name\n";
    open(ENTRIES, $entries_file_name) || die "Could not open file $entries_file_name";

    my(%cvs_entries);
    
    # Read in the path if available    
    while (defined ($line = <ENTRIES>))
    {
        chomp($line);
        
        #parse out the line. Format is:
        #   files:    /filename/version/date/options/tag
        #   dirs:     D/dirname////
        #   dir?      D
        # because we might be reading an entries file from another platform, with
        # different linebreaks, be anal about cleaning up $line.
                
        if ($line =~ /^?\/(.+)\/(.+)\/(.+)\/(.*)\/(.*)?$/)
        {
            my($filename) = $1;
            my($version) = $2;
            my($date) = $3;
            my($options) = $4;
            my($tag) = $5;
            
            my(%cvs_file) = (
                "version"   => $version,
                "date"      => $date,
                "options"   => $options,
                "tag"       => $tag
            );
                        
            # print $filename." ".$version." ".$date." ".$options." ".$tag."\n";
            $cvs_entries{$filename} = \%cvs_file;
        }
    }
   
    close ENTRIES;

    return %cvs_entries;
}


#//--------------------------------------------------------------------------------------------------
#// _fileIsModified. compare mod date with CVS entries to see if a file is modified
#//--------------------------------------------------------------------------------------------------
sub _fileIsModified($;$;$)
{
    my($entries, $dir, $file) = @_;

    my($abs_file) = $dir.$dirsep.$file;

    if (exists($entries->{$file}))
    {
        my($date) = $entries->{$file}->{"date"};
        
        # stat the file to get its date
        my($file_data) = stat($abs_file) || die "Could not stat $file\n";
        my($mod_string) = scalar(gmtime($file_data->mtime));

       return ($mod_string ne $date);
    }
    else
    {
        return 0;    
    }
}


#//--------------------------------------------------------------------------------------------------
#// _processFile. args: entries hash, dir, filename
#//--------------------------------------------------------------------------------------------------

sub _processFile($;$;$;$;$)
{
    my($src_entries, $dest_entries, $srcdir, $destdir, $file) = @_;

    my($abs_file) = $srcdir.$dirsep.$file;
            
    if (exists($src_entries->{$file}))
    {
        my($file_entry) = $src_entries->{$file};
        my($version) = $file_entry->{"version"};
        
        if (_fileIsModified($src_entries, $srcdir, $file))
        {
            my($rel_file) = $abs_file;
            $rel_file =~ s/^$src_tree//;
            
            # print "¥ÊFile $rel_file is modified\n";
            
            # check CVS version in dest
            my($dest_entry) = $dest_entries->{$file};
            if ($dest_entry)
            {
                my($dest_version) = $dest_entry->{"version"};
                my($versions_match) = ($version == $dest_version);
                my($dest_modified) = _fileIsModified($dest_entries, $destdir, $file);
                                
                if ($versions_match)
                {
                    # ok, we can copy the file over now, backing up dest if it is modified
                    _copyFile($srcdir, $destdir, $file, $dest_modified);
                }
                else
                {
                    print "File $rel_file is version $version in the src tree, but $dest_version in dest. This file will not be copied.\n";
                }
            }
            else
            {
                print "No CVS entry found in destination tree for $rel_file\n";
            }
        }
    }
    else
    {
        print "No entry for file $file\n";
    }
}


#//--------------------------------------------------------------------------------------------------
#// _traverseDir. Traverse one dir, recurse for each found dir.
#//--------------------------------------------------------------------------------------------------

sub _traverseDir($;$)
{
    my($srcdir, $destdir) = @_;

    opendir(DIR, $srcdir) or die "Cannot open dir $srcdir\n";
    my @files = readdir(DIR);
    closedir DIR;
    
    # suck in the CVS info for this dir, if there is a CVS dir
    unless (-e $srcdir.$dirsep."CVS:Entries" && -e $destdir.$dirsep."CVS:Entries") {
        print "$srcdir is not a CVS directory in both source and dest\n";
        return;
    }
    
    print " Doing $srcdir\n";
    
    my(%src_entries)  = _readCVSInfo($srcdir);
    my(%dest_entries) = _readCVSInfo($destdir);

    my $file;    
    foreach $file (@files)
    {        
        my $filepath = $srcdir.$dirsep.$file;
                
        if (-d $filepath)
        {
            if ($file ne "CVS")     # ignore 'CVS' dirs
            {
                # print "Going into $filepath\n";            
                _traverseDir($filepath, $destdir.$dirsep.$file);
            }
        }
        else
        {
            # process this file
            _processFile(\%src_entries, \%dest_entries, $srcdir, $destdir, $file);
        }
    }
}

#//--------------------------------------------------------------------------------------------------
#// MigrateChanges
#//--------------------------------------------------------------------------------------------------
sub MigrateChanges($;$)
{
    my($srcdir, $destdir) = @_;

    # Check that src and dest exist
    if (! -d $srcdir) {
        die "Source directory $srcdir does not exist\n";
    }

    if (! -d $destdir) {
        die "Dest directory $destdir does not exist\n";
    }

    print "---------------------------------------------------------\n";
    print "Migrating changes from\n  $srcdir\nto\n  $destdir\n";
    print "---------------------------------------------------------\n";
    _traverseDir($srcdir, $destdir);
    print "---------------------------------------------------------\n";
}


MigrateChanges($src_tree, $dest_tree);

