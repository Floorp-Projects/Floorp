#!/perl

# make-jars [-c] [-v] [-d <destPath>] [-s <srcdir>] < <jar.mn>

use strict;

use Getopt::Std;
use Cwd;
use File::stat;
use Time::localtime;
use Cwd;
use File::Copy;
use File::Path;

my @cleanupList = ();
my $IS_DIR = 1;
my $IS_FILE = 2;

my $objdir = getcwd;

getopts("d:s:cv");

my $usrcdir = undef;
if (defined($::opt_s)) {
    $usrcdir = $::opt_s;
}

my $destPath = ".";
if (defined($::opt_d)) {
    $destPath = $::opt_d;
    if ( defined($usrcdir) && substr($destPath, 0, 1) ne "/") {
	$destPath = "$objdir/$destPath";
    }
}

my $copyFiles = 0;
if (defined($::opt_c)) {
    $copyFiles = 1;
}

my $verbose = 0;
if (defined($::opt_v)) {
    $verbose = 1;
}

if ($verbose) {
    print "make-jars "
        . ($copyFiles ? "-c " : "")
#        . ($srcDir ? "-s $srcDir " : "")
        . "-v -d $destPath\n";
}

sub Cleanup
{
    while (1) {
        my $isDir = pop(@cleanupList);
        if ($isDir == undef) {
            return 0;
        }
        my $path = pop(@cleanupList);
	#print "Cleaning up $path\n";
        if ($isDir == $IS_DIR) {
            rmdir($path) || die "can't remove dir $path: $!";
        }
        else {
            unlink($path) || die "can't remove file $path: $!";
        }
    }
}

sub zipErrorCheck($)
{
    my ($err) = @_;

    return if ($err == 0 || $err == 12);

    die ("Error invoking zip: $err\n");
	
}

sub JarIt
{
    my ($destPath, $jarfile, $copyFiles, $args, $overrides) = @_;

    my $dir = $destPath;
    if ("$dir/$jarfile" =~ /([\w\d.\-\\\/]+)[\\\/]([\w\d.\-]+)/) {
        $dir = $1;
    }
    
    MkDirs($dir, ".", 0);

    if ($copyFiles) {
        my $indivDir = $jarfile;
        if ($indivDir =~ /(.*).jar/) {
            $indivDir = $1;
        }
        my $indivPath = "$destPath/$indivDir";
        MkDirs($indivPath, ".", 0);

	my $file;
        foreach $file (split(' ', $args)) {
            if ($verbose) {
                print "adding individual file $file to dist\n";
            }
            EnsureFileInDir("$indivPath/$file", $file, 0, 0);
        }
        foreach $file (split(' ', $overrides)) {
            if ($verbose) {
                print "override: adding individual file $file to dist\n";
            }
            EnsureFileInDir("$indivPath/$file", $file, 0, 1);
        }
    }

    if (!($args eq "")) {
	my $cwd = getcwd;
	my $err = 0; 
        system("zip -u $destPath/$jarfile $args") == 0 or
	    $err = $? >> 8;
	zipErrorCheck($err);
    }
    if (!($overrides eq "")) {
	my $err = 0; 
        print "+++ overriding $overrides\n";
        system("zip $destPath/$jarfile $overrides\n") == 0 or 
	    $err = $? >> 8;
	zipErrorCheck($err);
    }

    Cleanup();
}

sub MkDirs
{
    my ($path, $containingDir, $doCleanup) = @_;
    #print "MkDirs $path $containingDir $doCleanup\n";
    if ($path =~ /([\w\d.\-]+)[\\\/](.*)/) {
        my $dir = $1;
        $path = $2;
        if (!-e $dir) {
            if ($verbose) {
                print "making dir $containingDir/$dir\n";
            }
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
            if ($doCleanup) {
                push(@cleanupList, "$containingDir/$dir");
                push(@cleanupList, $IS_DIR);
            }
        }
        my $wd = cwd();
        chdir $dir;
        MkDirs($path, "$containingDir/$dir", $doCleanup);
        chdir $wd;
    }
    else {
        my $dir = $path;
        if ($dir eq "") { return 0; } 
        if (!-e $dir) {
            if ($verbose) {
                print "making dir $containingDir/$dir\n";
            }
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
            if ($doCleanup) {
                push(@cleanupList, "$containingDir/$dir");
                push(@cleanupList, $IS_DIR);
            }
        }
    }
}

sub CopyFile
{
    my ($from, $to, $doCleanup) = @_;
    if ($verbose) {
        print "copying $from to $to\n";
    }
    copy("$from", "$to") || die "CopyFile failed: $!\n";

    # fix the mod date so we don't jar everything (is this faster than just jarring everything?)
    my $atime = stat($from)->atime || die $!;
    my $mtime = stat($from)->mtime || die $!;
    utime($atime, $mtime, $to);

    if ($doCleanup) {
        push(@cleanupList, "$to");
        push(@cleanupList, $IS_FILE);
    }
}

sub EnsureFileInDir
{
    my ($destPath, $srcPath, $doCleanup, $override) = @_;

    #print "EnsureFileInDir($destPath, $srcPath, $doCleanup, $override)\n";
    my $destStat = stat($destPath);
    my $destMtime = $destStat ? $destStat->mtime : 0;
    my $srcStat = stat($srcPath);
    my $srcMtime = $srcStat ? $srcStat->mtime : 0;
    #print "destMtime = $destMtime, srcMtime = $srcMtime\n";
    if (!-e $destPath || $destMtime < $srcMtime || $override) {
        #print "copying $destPath, from $srcPath\n";
        my $dir = "";
        my $file;
        if ($destPath =~ /([\w\d.\-\\\/]+)[\\\/]([\w\d.\-]+)/) {
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
	    if (defined($usrcdir)) {
		if (-e "$usrcdir/$file") {
		    $file = "$usrcdir/$file";
		} else {
		    die "error: file '$usrcdir/$file' doesn't exist\n";
		}
	    } else {
		die "error: file '$file' doesn't exist\n";
	    }
        }
        MkDirs($dir, ".", $doCleanup);
        CopyFile($file, $destPath, $doCleanup);
        return 1;
    }
#    elsif ($doCleanup == 0 && -e $destPath) {
#        print "!!! file $destPath already exists -- need to add '+' rule to jar.mn\n";
#    }
    return 0;
}

if (defined($usrcdir)) {
    if ( -e "$usrcdir" ) {
	print "Change to $usrcdir from $objdir\n";
	chdir($usrcdir) or die "chdir: $!\n";
    } else {
	die "srcdir $usrcdir does not exist!\n";
    }
}

while (<STDIN>) {
    chomp;
  start: 
    if (/^([\w\d.\-\\\/]+)\:\s*$/) {
        my $jarfile = $1;
        my $args = "";
        my $overrides = "";
	my $cwd = cwd();
	print "+++ making chrome $cwd  => $destPath/$jarfile\n";
        while (<STDIN>) {
            if (/^\s+([\w\d.\-\\\/]+)\s*(\([\w\d.\-\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = $2;

                if ( $srcPath ) {  
                    $srcPath = substr($srcPath,1,-1);

		    if (defined($usrcdir)) {
			#print "Chkpt A: Switching to $objdir\n";
			chdir $objdir or die "chdir: $!\n";
			$srcPath = "$usrcdir/$srcPath";
		    }
		}
		EnsureFileInDir($dest, $srcPath, 1);
                $args = "$args$dest ";
		JarIt($destPath, $jarfile, $copyFiles, $args, $overrides);

		if ($srcPath && defined($usrcdir)) {
		    #print "Chkpt B: Switching back to $usrcdir\n";
		    chdir $usrcdir or die "chdir: $!\n";
		}		
            } elsif (/^\+\s+([\w\d.\-\\\/]+)\s*(\([\w\d.\-\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = $2;

                if ( $srcPath ) {  
                    $srcPath = substr($srcPath,1,-1);
		    if (defined($usrcdir)) {
			#print "Chkpt C: Switching to $objdir\n";
			chdir $objdir or die "chdir: $!\n";
			$srcPath = "$usrcdir/$srcPath";
		    }
		}
                EnsureFileInDir($dest, $srcPath, 1);
                $overrides = "$overrides$dest ";
		JarIt($destPath, $jarfile, $copyFiles, $args, $overrides);
		if ($srcPath && defined($usrcdir)) {
		    #print "Chkpt D: Switching back to $usrcdir\n";
		    chdir $usrcdir or die "chdir: $!\n";
		}		
            } elsif (/^\s*$/) {
                # end with blank line
                last;
            } else {
		#print "Chkpt E\n";

                JarIt($destPath, $jarfile, $copyFiles, $args, $overrides);

		#print "Chkpt F\n";

                goto start;
            }
        }

    } elsif (/^\s*\#.*$/) {
        # skip comments
    } elsif (/^\s*$/) {
        # skip blank lines
    } else {
        close;
        die "bad jar rule head at: $_";
    }
}
