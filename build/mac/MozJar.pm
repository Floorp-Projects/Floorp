#!perl -w
package MozJar;

#
# Module for creating jar files, either using a jar manifest, or
# simply jarring up folders on disk.
#

require 5.004;
require Exporter;

use strict;
use Archive::Zip;
use File::Path;

use Mac::Files;

use Moz;

use vars qw( @ISA @EXPORT );

@ISA        = qw(Exporter);
@EXPORT     = qw(CreateJarFileFromDirectory WriteOutJarFiles);


#-------------------------------------------------------------------------------
# Add the contents of a directory to the zip file
#
#-------------------------------------------------------------------------------
sub _addDirToJar($$$$)
{
    my($dir, $jar_root, $zip, $compress) = @_;

    opendir(DIR, $dir) or die "Cannot open dir $dir\n";
    my @files = readdir(DIR);
    closedir DIR;
    
    my $unix_jar_root = $jar_root;
    $unix_jar_root =~ s|:|/|g;       # colon to slash conversion
    
    my $file;
    
    foreach $file (@files)
    {        
        my $filepath = $dir.":".$file;
        
        if (-d $filepath)
        {
            print "Adding files to jar from $filepath\n";            
            _addDirToJar($filepath, $jar_root, $zip, $compress);
        }
        else
        {
            my $member = Archive::Zip::Member->newFromFile($filepath);
            die "Failed to create zip file member $filepath\n" unless $member;
            
            my $unixName = $filepath;
            $unixName =~ s|:|/|g;               # colon to slash conversion
            $unixName =~ s|^$unix_jar_root||;   # relativise
            
            $member->fileName($unixName);
            
            # print "Adding $file as $unixName\n";
            
            if ($compress) {
                $member->desiredCompressionMethod(Archive::Zip::COMPRESSION_DEFLATED);            
            } else {
                $member->desiredCompressionMethod(Archive::Zip::COMPRESSION_STORED);
            }
            
            $zip->addMember($member);
        }
    }
}

#-------------------------------------------------------------------------------
# Add the contents of a directory to the zip file
#
#-------------------------------------------------------------------------------

sub CreateJarFileFromDirectory($$$)
{
    my($srcdir, $jarpath, $compress) = @_;

    my $zip = Archive::Zip->new();

    _addDirToJar($srcdir, $srcdir, $zip, $compress);

    print "Saving zip file...\n";
    my $status = $zip->writeToFileNamed($jarpath);
    if ($status == 0) {
        print "Zipping completed successfully\n";
    } else {
        print "Error saving zip file\n";
    }
    
    # set the file type/creator to something reasonable
    MacPerl::SetFileInfo("ZIP ", "ZIP ", $jarpath);
}


#-------------------------------------------------------------------------------
# addToJarFile
# 
# Add a file to a jar file
#
# Parameters:
#   1. Jar ID. Unix path of jar file inside chrome.
#   2. Abs path to jar.mn file (i.e. source)    (mac breaks)
#   3. File source, relative to jar.mn path     (mac breaks)
#   4. Abs path to the resulting .jar file      (mac breaks)
#   5. Relative file path within the jar        (unix breaks)
#   6. Reference to hash of jar files
#
#-------------------------------------------------------------------------------

sub addToJarFile($$$$$$)
{
    my($jar_id, $jar_man_dir, $file_src, $jar_path, $file_jar_path, $jars) = @_;

    # print "addToJarFile with:\n  $jar_man_dir\n  $file_src\n  $jar_path\n  $file_jar_path\n";

    unless ($jar_path =~ m/(.+:)([^:]+)$/) { die "Bad jar path $jar_path\n"; }
    
    my($target_dir) = $1;
    my($jar_name) = $2;

    $target_dir =~ s/[^:]+$//;
    
    # print "¥ $target_dir $jar_name\n";

    # find the source file
    my($src) = $jar_man_dir.":".$file_src;
    if ((!-e $src) && ($file_src =~ m/.+:([^:]+)$/))    # src does not exist. Fall back to looking for src in jar.mn dir
    {
        $file_src = $1;
        $src = $jar_man_dir.":".$file_src;
        
        if (!-e $src) {
            die "Can't find chrome file $src\n";
        }
    }
    
    if ($main::options{jars})
    {
        my($zip) = $jars->{$jar_id};
        unless ($zip) { die "Can't find Zip entry for $jar_id\n"; }
        
        # print "Adding $file_src to jar file $jar_path at $file_jar_path\n";
    
        my($member) = Archive::Zip::Member->newFromFile($src);
        unless ($member) { die "Failed to create zip file member $src\n"; }

        $member->fileName($file_jar_path);    
    
        my($compress) = 1;
        if ($compress) {
            $member->desiredCompressionMethod(Archive::Zip::COMPRESSION_DEFLATED);            
        } else {
            $member->desiredCompressionMethod(Archive::Zip::COMPRESSION_STORED);
        }

        $zip->addMember($member);        
    }
    else        # copy file
    {
        my($rel_path) = $file_jar_path;
        $rel_path =~ s|/|:|g;       # slash to colons
        
        my($dir_name) = $jar_name;
        $dir_name =~ s/\.jar$//;
        
        my($dst) = $target_dir.$dir_name.":".$rel_path;
                
        # print "Aliassing $src\n to\n$dst\n";        
        MakeAlias($src, $dst);      # don't check errors, otherwise we fail on replacement
    }
}


#-------------------------------------------------------------------------------
# setupJarFile
# 
# setup a zip for writing
#-------------------------------------------------------------------------------

sub setupJarFile($$$)
{
    my($jar_id, $jar_path, $jar_hash) = @_;

    # print "Creating jar file $jar_id at $jar_path\n";
    
    if ($main::options{jars})
    {
        my($zip) = $jar_hash->{$jar_id};
        if (!$zip)      # if we haven't made it already, do so
        {
	        my($zip) = Archive::Zip->new();
	        $jar_hash->{$jar_id} = $zip;
        }
    }
    else
    {
        # installing files.
        # nothing to do. MakeAlias creates dirs as needed.

	    # add this jar to the list
	    $jar_hash->{$jar_id} = 1;
    }
}


#-------------------------------------------------------------------------------
# closeJarFile
# 
# We're done with this jar file _for this jar.mn_. We may add more entries
# to it later, so keep it open in the hash.
#-------------------------------------------------------------------------------
sub closeJarFile($$)
{
    my($jar_path, $jar_hash) = @_;

    # print "Closing jar file $jar_path\n";

    if ($main::options{jars})
    {
    
    }
    else
    {
        # installing files.
        # nothing to do
    }
}


#-------------------------------------------------------------------------------
# WriteOutJarFiles
# 
# Now we dump out the jars
#-------------------------------------------------------------------------------
sub WriteOutJarFiles($$)
{
    my($chrome_dir, $jars) = @_;

    unless ($main::options{jars}) { return; }
    
    my($full_chrome_path) = Moz::full_path_to($chrome_dir);

    my($key);
    foreach $key (keys %$jars)
    {
        my($zip) = $jars->{$key};
        
        my($rel_path) = $key;
        $rel_path =~ s/\//:/g;
        
        my($output_path) = $full_chrome_path.":".$rel_path;
        
        print "Writing zip file $key to $output_path\n";

        # ensure the target dirs exist
        my($path) = $output_path;
        $path =~ s/\.jar$//;
        mkpath($path);
        
        ($zip->writeToFileNamed($output_path) == Archive::Zip::AZ_OK) || die "Error writing jar $rel_path\n";
        
        MacPerl::SetFileInfo("ZIP ", "ZIP ", $output_path);
    }
}


#-------------------------------------------------------------------------------
# registerChromePackage
# 
# Enter a chrome package into the installed-chrome.txt file
#-------------------------------------------------------------------------------
sub registerChromePackage($$$$)
{
    my($jar_file, $file_path, $chrome_dir, $jar_hash) = @_;

    my($manifest_subdir) = $jar_file;
    $manifest_subdir =~ s/:/\//g;
    
    my($chrome_entry);
    
    if ($main::options{jars}) {
        $chrome_entry = ",install,url,jar:resource:/Chrome/";
        $manifest_subdir.= "!/";
    } else {
        $chrome_entry = ",install,url,resource:/Chrome/";
        $manifest_subdir =~ s/\.jar$/\//;
    }

    # print "Entering $chrome_entry$manifest_subdir in installed-chrome.txt\n";

    # for now, regiser for content, locale and skin
    # we'll get the type from the path soon
    my($type) = "content";
    
    # ensure chrome_dir exists
    mkpath($chrome_dir);
    
    my($inst_chrome) = ${chrome_dir}.":installed-chrome.txt";
    
    open(CHROMEFILE, ">>${inst_chrome}") || die "Failed to open $inst_chrome\n";
     
    print(CHROMEFILE "${type}${chrome_entry}${manifest_subdir}\n");
    $type = "locale";
    print(CHROMEFILE "${type}${chrome_entry}${manifest_subdir}\n");
    $type = "skin";
    print(CHROMEFILE "${type}${chrome_entry}${manifest_subdir}\n");

    close(CHROMEFILE);
}

#-------------------------------------------------------------------------------
# Create or add to a jar file from a jar.mn file.
# Both arguments are relative to the mozilla root dir.
#
#
#-------------------------------------------------------------------------------
sub CreateJarFromManifest($$$)
{
    my($jar_man_path, $dest_path, $jars) = @_;
    
    if ($main::options{jars}) {
        print "Jarring from $jar_man_path\n";
    } else {
        print "Installing files from $jar_man_path\n";
    }

    $jar_man_path = Moz::full_path_to($jar_man_path);
    $dest_path = Moz::full_path_to($dest_path);

    # if the jars hash is empty, nuke installed-chrome.txt
    if (! scalar(%$jars))
    {
        print "Nuking chrome\n";
	    my($installed_chrome) = $dest_path.":installed-chrome.txt";
	    # unlink $installed_chrome;
    }
    
    my $jar_man_dir = "";
    my $jar_man_file = "";
    
    if ($jar_man_path =~ /(.+):([^:]+)$/)
    {
        $jar_man_dir = $1;      # no trailing :
        $jar_man_file = $2;
    }

    # Keep a hash of jar files, keyed on relative jar path (e.g. "packages/core.jar")
    # Entries are open Archive::Zips (if zipping), and installed-chrome entries.
    
    my($jar_id) = "";       # Current foo/bar.jar from jar.mn file
    my($jar_file) = "";     # relative path to jar file (from $dest_path), with mac separators
    my($full_jar_path);
    
    open(FILE, "<$jar_man_path") || die "could not open \"$jar_man_path\": $!";
    while (<FILE>)
    {
        my($line) = $_;
        chomp($line);
        
        # print "$line\n";
        
        if ($line =~ /^\s*\#.*$/) {     # skip comments
            next;
        }
        
        if ($line =~/^([\w\d.\-\\\/]+)\:\s*$/)                                  # line start jar file entries
        {
            $jar_id = $1;
            $jar_file = $jar_id;
            $jar_file =~ s|/|:|g;           # slash to colons
            $full_jar_path = $dest_path.":".$jar_file;
                        
            setupJarFile($jar_id, $full_jar_path, $jars);

        }
        elsif ($line =~ /^\s+([\w\d.\-\\\/]+)\s*(\([\w\d.\-\\\/]+\))?$\s*/)     # jar file entry
        {
            my($file_dest) = $1;
            my($file_src) = $2;
 
            if ($file_src) {  
                $file_src = substr($file_src, 1, -1);      #strip the ()
            } else {
                $file_src = $file_dest;
            }
            
            $file_src =~ s|/|:|g;
            
            if ($jar_file ne "")    # if jar is open, add to jar
            {
                if ($file_dest eq "manifest.rdf")   # will change to contents.rdf
                {
                    registerChromePackage($jar_file, $file_dest, $dest_path, $jars);
                }
                
                addToJarFile($jar_id, $jar_man_dir, $file_src, $full_jar_path, $file_dest, $jars);
            }
            else
            {
                die "bad jar.mn format at $line\n";
            }
        }
        elsif ($line =~ /^\s*$/ )                                               # blank line
        {
            if ($jar_file ne "")    #if a jar file is open, close it
            {
                closeJarFile($full_jar_path, $jars);
                
                $jar_file = "";
                $full_jar_path = "";
            }
        }
    }
    
    close(FILE);

    if ($jar_file ne "")    #if a jar file is open, close it
    {
        closeJarFile($full_jar_path, $jars);
    }

}

1;
