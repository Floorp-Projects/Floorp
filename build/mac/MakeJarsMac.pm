#!perl -w
# make-jars [-d <destPath>] < <manifest.jr>

package	MozJar;

require 5.004;

use strict;
use Cwd;
use Archive::Zip qw( :ERROR_CODES :CONSTANTS );
use Moz;

use vars qw( @ISA @EXPORT );

@ISA		= qw(Exporter);
@EXPORT     = qw(ProcessJarManifest);

sub _addToJar($$$$)
{
      my($thing, $thingDir, $zip, $compress) = @_;
      #print "_addToJar($thing, $thingDir, $zip, $compress)\n";
      
      my $filepath = "$thingDir/$thing";
      $filepath =~ s|/|:|g;
      if (-d $filepath) {
              my $dir = $filepath;
              $dir =~ s|/|:|g;
              opendir(DIR, $dir) or die "Cannot open dir $dir\n";
              my @files = readdir(DIR);
              closedir DIR;

              my $file;
              foreach $file (@files)
              {        
                      _addToJar("$thing/$file", $thingDir, $zip, $compress);
              }
      }
      else {
              my $member = Archive::Zip::Member->newFromFile($filepath);
              die "Failed to create zip file member $filepath\n" unless $member;

              $member->fileName($thing);

              print "Adding $filepath as $thing\n";

              if ($compress) {
                      $member->desiredCompressionMethod(Archive::Zip::COMPRESSION_DEFLATED);            
              } else {
                      $member->desiredCompressionMethod(Archive::Zip::COMPRESSION_STORED);
              }

              $zip->addMember($member);
      }
}

sub JarIt($$)
{
    my ($jarfile, $zip) = @_;
    #print "+++ jarring $jarfile\n";
    #flush();
    #system "zip -u $jarfile $args\n";
    my $jarTempFile = $jarfile . ".temp";
    $zip->writeToFileNamed($jarTempFile) == AZ_OK
      || die "zip writeToFileNamed $jarTempFile failed";

    # set the file type/creator to something reasonable
    MacPerl::SetFileInfo("ZIP ", "ZIP ", $jarTempFile);
    rename($jarTempFile, $jarfile);
    print "+++ finished jarring $jarfile\n";
}

sub MkDirs($)
{
    my ($path) = @_;
    #print "MkDirs $path\n";
    if ($path =~ /([\w\d.\-]+)[:\\\/](.*)/) {
        my $dir = $1;
        $path = $2;
        if (!-e $dir) {
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
        }
        chdir $dir;
        MkDirs($path);
        chdir "::";
    }
    else {
        my $dir = $path;
        if ($dir eq "") { return 0; } 
        if (!-e $dir) {
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
        }
    }
}

sub CopyFile($$)
{
    my ($from, $to) = @_;
    #print "CopyFile($from, $to)\n";
    open(OUT, ">$to") || die "error: can't open '$to': $!";
    open(IN, "<$from") || die "error: can't open '$from': $!";
    while (<IN>) {
        print OUT $_;
    }
    close(IN) || die "error: can't close '$from': $!";
    close(OUT) || die "error: can't close '$to': $!";
}

sub EnsureFileInDir($$)
{
    my ($destPath, $srcPath) = @_;
    $destPath =~ s|/|:|g;
    $destPath = ":$destPath";
    $srcPath =~ s|/|:|g;
    $srcPath = ":$srcPath";

    #print "EnsureFileInDir($destPath, $srcPath)\n";
    if (!-e $destPath) {
        my $dir = "";
        my $file;
        if ($destPath =~ /([\w\d.:\-\\\/]+)[:\\\/]([\w\d.\-]+)/) {
            $dir = $1;
            $file = $2;
        }
        else {
            $file = $destPath;
        }

        if ($srcPath) {
            $file = $srcPath;
        }

        if (!-e $file) {
            die "error: file '$file' doesn't exist\n";
        }
        MkDirs($dir);
        CopyFile($file, $destPath);
        return 1;
    }
    return 0;
}

sub ProcessJarManifest($$)
{
    my ($jarManPath, $destPath) = @_;
    
    $jarManPath = Moz::full_path_to($jarManPath);
    $destPath = Moz::full_path_to($destPath);
    #print "ProcessJarManifest($jarManPath, $destPath)\n";

    print "+++ jarring $jarManPath\n";
    
	my $jarManDir = "";
	my $jarManFile = "";
	if ($jarManPath =~ /([\w\d.:\-\\\/]+)[:\\\/]([\w\d.\-]+)/) {
        $jarManDir = $1;
        $jarManFile = $2;
    }
    else {
    	die "bad jar.mn specification";
    }

	my $prevDir = cwd();
    chdir($jarManDir);

    open(FILE, "<$jarManPath") || die "could not open $jarManPath: $!";
	while (<FILE>) {
	    chomp;
	  start: 
	    if (/^([\w\d.\-\\\/]+)\:\s*$/) {
	        my $jarfile = "$destPath/$1";
	        $jarfile =~ s|/|:|g;
	        #my $args = "";

            my $zip = Archive::Zip->new();
            #print "new jar $jarfile\n";
	        if (-e $jarfile) {
	        	#print "=====> $jarfile exists\n";
	        	my $ok = $zip->read($jarfile);
	        	if ($ok != AZ_OK) {
	        	    die "zip read $jarfile failed: $ok";
	        	}
	        }

	        while (<FILE>) {
	            if (/^\s+([\w\d.\-\\\/]+)\s*(\([\w\d.\-\\\/]+\))?$\s*/) {
					my $dest = $1;
					my $srcPath = $2;

                	if ( $srcPath ) {  
                	    $srcPath = substr($srcPath,1,-1);
                	}

	                EnsureFileInDir($dest, $srcPath);
	                #$args = "$args$dest ";
	                _addToJar($dest, $jarManDir, $zip, 1);
	            } elsif (/^\s*$/) {
	                # end with blank line
	                last;
	            } else {
	                JarIt($jarfile, $zip);
	                goto start;
	            }
	        }
	        JarIt($jarfile, $zip);

	    } elsif (/^\s*\#.*$/) {
	        # skip comments
	    } elsif (/^\s*$/) {
	        # skip blank lines
	    } else {
	        close;
	        die "bad jar rule head at: $_";
	    }
	}
    close(FILE);
    chdir($prevDir);
}

