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

sub _addToJar($$$$$)
{
      my($thing, $srcPath, $jarManDir, $zip, $compress) = @_;
      #print "_addToJar($thing, $srcPath, $jarManDir, $zip, $compress)\n";
      
      my $existingMember = $zip->memberNamed($thing);
      if ($existingMember) {
          my $modtime = $existingMember->lastModTime();
          print "already have $thing at $modtime\n";  # XXX need to check mod time here!
          return 0;
      }
      
      my $filepath = "$jarManDir:$srcPath";
      $filepath =~ s|/|:|g;
      
      if (!-e $filepath) {
          $srcPath =~ /([\w\d.:\-\\\/]+)[:\\\/]([\w\d.\-]+)/;
          $filepath = "$jarManDir:$2";
          if (!-e $filepath) {
              die "$filepath does not exist\n";
          }
      }
      
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
                	else {
                		$srcPath = ":" . $dest;
                	}
                	$srcPath =~ s|/|:|g;

	                _addToJar($dest, $srcPath, $jarManDir, $zip, 1);
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
}

