#!perl -w
package	MozJar;

require 5.004;


use strict;
use Archive::Zip;

use vars qw( @ISA @EXPORT );

@ISA		= qw(Exporter);
@EXPORT 	= qw(CreateJarFile);


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

sub CreateJarFile($$)
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
