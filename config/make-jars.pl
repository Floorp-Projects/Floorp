#!/perl

# make-jars [-f] [-v] [-l] [-d <chromeDir>] [-s <srcdir>] < <jar.mn>

if ($^O ne "cygwin") {
# we'll be pulling in some stuff from the script directory
require FindBin;
import FindBin;
push @INC, $FindBin::Bin;
}

use strict;

use Getopt::Std;
use Cwd;
use File::stat;
use Time::localtime;
use Cwd;
use File::Copy;
use File::Path;
use IO::File;
require mozLock;
import mozLock;

my $objdir = getcwd;

getopts("d:s:f:avlD:p:");

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

my $fileformat = "jar";
if (defined($::opt_f)) {
    ($fileformat = $::opt_f) =~ tr/A-Z/a-z/;
}

if ("$fileformat" ne "jar" &&
    "$fileformat" ne "flat" &&
    "$fileformat" ne "both") {
    print "File format specified by -f option must be one of: jar, flat, or both.\n";
    exit(1);
}

my $zipmoveopt = "";
if ("$fileformat" eq "jar") {
    $zipmoveopt = "-m";
}

my $nofilelocks = 0;
if (defined($::opt_l)) {
    $nofilelocks = 1;
}

my $autoreg = 1;
if (defined($::opt_a)) {
    $autoreg = 0;
}

my $preprocessor = "";
if (defined($::opt_p)) {
    $preprocessor = $::opt_p;
}

my $defines = "";
while (@ARGV) {
    $defines = "$defines ".shift(@ARGV);
}

if ($verbose) {
    print "make-jars "
        . "-v -d $chromeDir "
        . ($fileformat ? "-f $fileformat " : "")
        . ($nofilelocks ? "-l " : "")
        . ($baseFilesDir ? "-s $baseFilesDir " : "")
        . "\n";
}

my $win32 = ($^O =~ /((MS)?win32)|cygwin|os2/i) ? 1 : 0;
my $macos = ($^O =~ /MacOS|darwin/i) ? 1 : 0;
my $unix  = !($win32 || $macos) ? 1 : 0;

sub foreignPlatformFile
{
   my ($jarfile) = @_;
   
   if (!$win32 && index($jarfile, "-win") != -1) {
     return 1;
   }
   
   if (!$unix && index($jarfile, "-unix") != -1) {
     return 1; 
   }

   if (!$macos && index($jarfile, "-mac") != -1) {
     return 1;
   }

   return 0;
}

sub zipErrorCheck($$)
{
    my ($err,$lockfile) = @_;
    return if ($err == 0 || $err == 12);
    mozUnlock($lockfile) if (!$nofilelocks);
    die ("Error invoking zip: $err");
}

sub JarIt
{
    my ($destPath, $jarfile, $args, $overrides) = @_;
    my $oldDir = cwd();
    chdir("$destPath/$jarfile");

    if ("$fileformat" eq "flat") {
	unlink("../$jarfile.jar") if ( -e "../$jarfile.jar");
	chdir($oldDir);
        return 0;
    }

    #print "cd $destPath/$jarfile\n";

    my $lockfile = "../$jarfile.lck";

    mozLock($lockfile) if (!$nofilelocks);

    if (!($args eq "")) {
	my $cwd = getcwd;
	my $err = 0; 

        #print "zip $zipmoveopt -u ../$jarfile.jar $args\n";

	# Handle posix cmdline limits (4096)
	while (length($args) > 4000) {
	    #print "Exceeding POSIX cmdline limit: " . length($args) . "\n";
	    my $subargs = substr($args, 0, 3999);
	    my $pos = rindex($subargs, " ");
	    $subargs = substr($args, 0, $pos);
	    $args = substr($args, $pos);
	    
	    #print "zip $zipmoveopt -u ../$jarfile.jar $subargs\n";	    
	    #print "Length of subargs: " . length($subargs) . "\n";
	    system("zip $zipmoveopt -u ../$jarfile.jar $subargs") == 0 or
		$err = $? >> 8;
	    zipErrorCheck($err,$lockfile);
	}
	#print "Length of args: " . length($args) . "\n";
        #print "zip $zipmoveopt -u ../$jarfile.jar $args\n";
        system("zip $zipmoveopt -u ../$jarfile.jar $args") == 0 or
	    $err = $? >> 8;
	zipErrorCheck($err,$lockfile);
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
	    
	    #print "zip $zipmoveopt ../$jarfile.jar $subargs\n";	    
	    #print "Length of subargs: " . length($subargs) . "\n";
	    system("zip $zipmoveopt ../$jarfile.jar $subargs") == 0 or
		$err = $? >> 8;
	    zipErrorCheck($err,$lockfile);
	}
        #print "zip $zipmoveopt ../$jarfile.jar $overrides\n";
        system("zip $zipmoveopt ../$jarfile.jar $overrides\n") == 0 or 
	    $err = $? >> 8;
	zipErrorCheck($err,$lockfile);
    }
    mozUnlock($lockfile) if (!$nofilelocks);
    chdir($oldDir);
    #print "cd $oldDir\n";
}


sub RegIt
{
    my ($chromeDir, $jarFileName, $chromeType, $pkgName) = @_;\
    chop($pkgName) if ($pkgName =~ m/\/$/);
    #print "RegIt:  $chromeDir, $jarFileName, $chromeType, $pkgName\n";

    my $line;
    if ($fileformat eq "flat")  {
	$line = "$chromeType,install,url,resource:/chrome/$jarFileName/$chromeType/$pkgName/";
    } else {
	$line = "$chromeType,install,url,jar:resource:/chrome/$jarFileName.jar!/$chromeType/$pkgName/";
    }
    my $installedChromeFile = "$chromeDir/installed-chrome.txt";
    my $lockfile = "$installedChromeFile.lck";
    mozLock($lockfile) if (!$nofilelocks);
    my $err = 0;
    if (open(FILE, "<$installedChromeFile")) {
	while (<FILE>) {
	    chomp;
	    if ($_ =~ $line) {
		# line already appears in installed-chrome.txt file
		# just update the mod date
		close(FILE) or $err = 1; 
		if ($err) {
		    mozUnlock($lockfile) if (!$nofilelocks);
		    die "error: can't close $installedChromeFile: $!";
		}
		my $now = time;
		utime($now, $now, $installedChromeFile) or $err = 1;
		mozUnlock($lockfile) if (!$nofilelocks);
		if ($err) {
		    die "couldn't touch $installedChromeFile";
		}
		print "+++ updating chrome $installedChromeFile\n+++\t$line\n";
		return;
	    }
	}
	close(FILE) or $err = 1;
	if ($err) {
	    mozUnlock($lockfile) if (!$nofilelocks);
	    die "error: can't close $installedChromeFile: $!";
	}
    }
    mozUnlock($lockfile) if (!$nofilelocks);
    
    my $dir = $installedChromeFile;
    if ("$dir" =~ /([\w\d.\-\\\/\+]+)[\\\/]([\w\d.\-]+)/) {
	$dir = $1;
    }
    mkpath($dir, 0, 0755);
    
    mozLock($lockfile) if (!$nofilelocks);
    $err = 0;
    open(FILE, ">>$installedChromeFile") or $err = 1;
    if ($err) {
	mozUnlock($lockfile) if (!$nofilelocks);
	die "can't open $installedChromeFile: $!";
    }
    print FILE "$line\n";
    close(FILE) or $err = 1;
    mozUnlock($lockfile) if (!$nofilelocks);
    if ($err) {
	die "error: can't close $installedChromeFile: $!";
    }
    print "+++ adding chrome $installedChromeFile\n+++\t$line\n";
}

sub EnsureFileInDir
{
    my ($destPath, $srcPath, $destFile, $srcFile, $override, $preproc) = @_;

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
            if ($destFile =~ /([\w\d.\-\_\\\/\+]+)[\\\/]([\w\d.\-\_]+)/) {
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
        if ($destPath =~ /([\w\d.\-\_\\\/\+]+)[\\\/]([\w\d.\-\_]+)/) {
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
        if ($preproc) {
            if (system("$^X $preprocessor $defines < $file > $destPath") != 0) {
                die "Preprocessing of $file failed: $!";
            }
        } else {
            copy($file, $destPath) || die "copy($file, $destPath) failed: $!";
        }

        # fix the mod date so we don't jar everything (is this faster than just jarring everything?)
        my $mtime = stat($file)->mtime || die $!;
        my $atime = stat($file)->atime;
        $atime = $mtime if !defined($atime);
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
            if (/^\s+([\w\d.\-\_\\\/\+]+)\s*(\([\w\d.\-\_\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = defined($2) ? substr($2, 1, -1) : $2;
		EnsureFileInDir("$chromeDir/$jarfile", $baseFilesDir, $dest, $srcPath, 0, 0);
                $args = "$args$dest ";
		if (!foreignPlatformFile($jarfile)  && $autoreg && $dest =~ /([\w\d.\-\_\+]+)\/([\w\d.\-\_\\\/]+)contents.rdf/)
		{
		    my $chrome_type = $1;
		    my $pkg_name = $2;
		    RegIt($chromeDir, $jarfile, $chrome_type, $pkg_name);
		}
            } elsif (/^\+\s+([\w\d.\-\_\\\/\+]+)\s*(\([\w\d.\-\_\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = defined($2) ? substr($2, 1, -1) : $2;
                EnsureFileInDir("$chromeDir/$jarfile", $baseFilesDir, $dest, $srcPath, 1, 0);
                $overrides = "$overrides$dest ";
		if (!foreignPlatformFile($jarfile)  && $autoreg && $dest =~ /([\w\d.\-\_\+]+)\/([\w\d.\-\_\\\/]+)contents.rdf/)
		{
		    my $chrome_type = $1;
		    my $pkg_name = $2;
		    RegIt($chromeDir, $jarfile, $chrome_type, $pkg_name);
		}
	    } elsif (/^\*\s+([\w\d.\-\_\\\/\+]+)\s*(\([\w\d.\-\_\\\/]+\))?$\s*/) {
		# preprocessed, no override
		my $dest = $1;
		my $srcPath = defined($2) ? substr($2, 1, -1) : $2;
		EnsureFileInDir("$chromeDir/$jarfile", $baseFilesDir, $dest, $srcPath, 0, 1);
		$args = "$args$dest ";
	    } elsif (/^\*\+\s+([\w\d.\-\_\\\/\+]+)\s*(\([\w\d.\-\_\\\/]+\))?$\s*/) {
		# preprocessed, override
		my $dest = $1;
		my $srcPath = defined($2) ? substr($2, 1, -1) : $2;
		EnsureFileInDir("$chromeDir/$jarfile", $baseFilesDir, $dest, $srcPath, 1, 1);
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
