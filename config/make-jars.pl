#!/perl

# make-jars [-f] [-v] [-d <chromeDir>] [-s <srcdir>] < <jar.mn>

use strict;

use Getopt::Std;
use Cwd;
use File::stat;
use Time::localtime;
use Cwd;
use File::Copy;
use File::Path;

my $objdir = getcwd;

getopts("d:s:vf");

my $baseFilesDir = ".";
if (defined($::opt_s)) {
    $baseFilesDir = $::opt_s;
}

my $chromeDir = ".";
if (defined($::opt_d)) {
    $chromeDir = $::opt_d;
}

my $verbose = 0;
if (defined($::opt_v)) {
    $verbose = 1;
}

my $flatfilesonly = 0;
if (defined($::opt_f)) {
    $flatfilesonly = 1;
}

if ($verbose) {
    print "make-jars "
        . "-v -d $chromeDir "
        . ($flatfilesonly ? "-f " : "")
        . ($baseFilesDir ? "-s $baseFilesDir " : "")
        . "\n";
}

sub zipErrorCheck($)
{
    my ($err) = @_;
    return if ($err == 0 || $err == 12);
    die ("Error invoking zip: $err");
}

sub JarIt
{
    my ($destPath, $jarfile, $args, $overrides) = @_;

    if ($flatfilesonly) {
        return 0;
    }

    my $oldDir = cwd();
    chdir("$destPath/$jarfile");
    #print "cd $destPath/$jarfile\n";

    if (!($args eq "")) {
	my $cwd = getcwd;
	my $err = 0; 
        #print "zip -u ../$jarfile.jar $args\n";

	# Handle posix cmdline limits (4096)
	while (length($args) > 4000) {
	    #print "Exceeding POSIX cmdline limit: " . length($args) . "\n";
	    my $subargs = substr($args, 0, 3999);
	    my $pos = rindex($subargs, " ");
	    $subargs = substr($args, 0, $pos);
	    $args = substr($args, $pos);
	    
	    #print "zip -u ../$jarfile.jar $subargs\n";	    
	    #print "Length of subargs: " . length($subargs) . "\n";
	    system("zip -u ../$jarfile.jar $subargs") == 0 or
		$err = $? >> 8;
	    zipErrorCheck($err);
	}
	#print "Length of args: " . length($args) . "\n";
        #print "zip -u ../$jarfile.jar $args\n";
        system("zip -u ../$jarfile.jar $args") == 0 or
	    $err = $? >> 8;
	zipErrorCheck($err);
    }

    if (!($overrides eq "")) {
	my $err = 0; 
        print "+++ overriding $overrides\n";

	while (length($args) > 4000) {
	    #print "Exceeding POSIX cmdline limit: " . length($args) . "\n";
	    my $subargs = substr($args, 0, 3999);
	    my $pos = rindex($subargs, " ");
	    $subargs = substr($args, 0, $pos);
	    $args = substr($args, $pos);
	    
	    #print "zip ../$jarfile.jar $subargs\n";	    
	    #print "Length of subargs: " . length($subargs) . "\n";
	    system("zip ../$jarfile.jar $subargs") == 0 or
		$err = $? >> 8;
	    zipErrorCheck($err);
	}
        #print "zip ../$jarfile.jar $overrides\n";
        system("zip ../$jarfile.jar $overrides\n") == 0 or 
	    $err = $? >> 8;
	zipErrorCheck($err);
    }

    chdir($oldDir);
    #print "cd $oldDir\n";
}

sub EnsureFileInDir
{
    my ($destPath, $srcPath, $destFile, $srcFile, $override) = @_;

    #print "EnsureFileInDir($destPath, $srcPath, $destFile, $srcFile, $override)\n";

    my $src = $srcFile;
    if (defined($src)) {
	if (! -e $src ) {
        	$src = "$srcPath/$srcFile";
	}
    }
    else {
        $src = "$srcPath/$destFile";
        # check for the complete jar path in the dest dir
        if (!-e $src) {
            #else check for just the file name in the dest dir
            my $dir = "";
            my $file;
            if ($destFile =~ /([\w\d.\-\_\\\/]+)[\\\/]([\w\d.\-\_]+)/) {
                $dir = $1;
                $file = $2;
            }
            else {
                die "file not found: $srcPath/$destFile";
            }
            $src = "$srcPath/$file";
            if (!-e $src) {
                die "file not found: $srcPath/$destFile";
            }
        }
    }

    $srcPath = $src;
    $destPath = "$destPath/$destFile";

    my $srcStat = stat($srcPath);
    my $srcMtime = $srcStat ? $srcStat->mtime : 0;

    my $destStat = stat($destPath);
    my $destMtime = $destStat ? $destStat->mtime : 0;
    #print "destMtime = $destMtime, srcMtime = $srcMtime\n";

    if (!-e $destPath || $destMtime < $srcMtime || $override) {
        #print "copying $destPath, from $srcPath\n";
        my $dir = "";
        my $file;
        if ($destPath =~ /([\w\d.\-\_\\\/]+)[\\\/]([\w\d.\-\_]+)/) {
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
            die "error: file '$file' doesn't exist";
        }
        if (!-e $dir) {
            mkpath($dir, 0, 0775) || die "can't mkpath $dir: $!";
        }
        unlink $destPath;       # in case we had a symlink on unix
        copy($file, $destPath) || die "copy($file, $destPath) failed: $!";

        # fix the mod date so we don't jar everything (is this faster than just jarring everything?)
        my $atime = stat($file)->atime || die $!;
        my $mtime = stat($file)->mtime || die $!;
        utime($atime, $mtime, $destPath);

        return 1;
    }
    return 0;
}

while (<STDIN>) {
    chomp;
  start: 
    if (/^([\w\d.\-\_\\\/]+).jar\:\s*$/) {
        my $jarfile = $1;
        my $args = "";
        my $overrides = "";
	my $cwd = cwd();
	print "+++ making chrome $cwd  => $chromeDir/$jarfile.jar\n";
        while (<STDIN>) {
            if (/^\s+([\w\d.\-\_\\\/]+)\s*(\([\w\d.\-\_\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = defined($2) ? substr($2, 1, -1) : $2;
		EnsureFileInDir("$chromeDir/$jarfile", $baseFilesDir, $dest, $srcPath, 0);
                $args = "$args$dest ";
            } elsif (/^\+\s+([\w\d.\-\_\\\/]+)\s*(\([\w\d.\-\_\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = defined($2) ? substr($2, 1, -1) : $2;
                EnsureFileInDir("$chromeDir/$jarfile", $baseFilesDir, $dest, $srcPath, 1);
                $overrides = "$overrides$dest ";
            } elsif (/^\s*$/) {
                # end with blank line
                last;
            } else {
                JarIt($chromeDir, $jarfile, $args, $overrides);
                goto start;
            }
        }
        JarIt($chromeDir, $jarfile, $args, $overrides);

    } elsif (/^\s*\#.*$/) {
        # skip comments
    } elsif (/^\s*$/) {
        # skip blank lines
    } else {
        close;
        die "bad jar rule head at: $_";
    }
}
