#!/perl

# make-jars [-f] [-v] [-l] [-x] [-d <chromeDir>] [-s <srcdir>] [-z zipprog] [-o operating-system] < <jar.mn>

my $cygwin_mountprefix = "";
if ($^O eq "cygwin") {
    $cygwin_mountprefix = $ENV{CYGDRIVE_MOUNT};
    if ($cygwin_mountprefix eq "") {
      $cygwin_mountprefix = `mount -p | awk '{ if (/^\\//) { print \$1; exit } }'`;
      if ($cygwin_mountprefix eq "") {
        print "Cannot determine cygwin mount points. Exiting.\n";
        exit(1);
      }
    }
    chomp($cygwin_mountprefix);
    # Remove extra ^M caused by using dos-mode line-endings
    chop $cygwin_mountprefix if (substr($cygwin_mountprefix, -1, 1) eq "\r");
} else {
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
use File::Copy;
use File::Path;
use File::Spec;
use IO::File;
use Config;
require mozLock;
import mozLock;

my $objdir = getcwd;

# if there's a "--", everything after it goes into $defines.  We don't do
# this with the remaining args in @ARGV after the getopts call because
# old versions of Getopt::Std don't understand "--".
my $ddindex = 0;
foreach my $arg (@ARGV) {
  ++$ddindex;
  last if ($arg eq "--");
}
my $defines = join(' ', @ARGV[ $ddindex .. $#ARGV ]);

getopts("d:s:f:avlD:o:p:xz:");

my $baseFilesDir = ".";
if (defined($::opt_s)) {
    $baseFilesDir = $::opt_s;
}

my $maxCmdline = 4000;
if ($Config{'archname'} =~ /VMS/) {
    $maxCmdline = 200;
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
    "$fileformat" ne "symlink" &&
    "$fileformat" ne "both") {
    print "File format specified by -f option must be one of: jar, flat, both, or symlink.\n";
    exit(1);
}

my $zipmoveopt = "";
if ("$fileformat" eq "jar") {
    $zipmoveopt = "-m -0";
}
if ("$fileformat" eq "both") {
    $zipmoveopt = "-0";
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

my $force_x11 = 0;
if (defined($::opt_x)) {
    $force_x11 = 1;
}

my $zipprog = $ENV{ZIP};
if (defined($::opt_z)) {
    $zipprog = $::opt_z;
}

if ($zipprog eq "") {
    print "A valid zip program must be given via the -z option or the ZIP environment variable. Exiting.\n";
    exit(1);
}

my $force_os;
if (defined($::opt_o)) {
    $force_os = $::opt_o;
}

if ($verbose) {
    print "make-jars "
        . "-v -d $chromeDir "
        . "-z $zipprog "
        . ($fileformat ? "-f $fileformat " : "")
        . ($nofilelocks ? "-l " : "")
        . ($baseFilesDir ? "-s $baseFilesDir " : "")
        . "\n";
}

my $win32 = ($^O =~ /((MS)?win32)|cygwin|os2/i) ? 1 : 0;
my $macos = ($^O =~ /MacOS|darwin/i) ? 1 : 0;
my $unix  = !($win32 || $macos) ? 1 : 0;
my $vms   = ($^O =~ /VMS/i) ? 1 : 0;

if ($force_x11) {
    $win32 = 0;
    $macos = 0;
    $unix = 1;
}

if (defined($force_os)) {
    $win32 = 0;
    $macos = 0;
    $unix = 0;
    if ($force_os eq "WINNT") {
    $win32 = 1;
    } elsif ($force_os eq "OS2") {
    $win32 = 1;
    } elsif ($force_os eq "Darwin") {
    $macos = 1;
    } else {
    $unix = 1;
    }
}

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

    if ("$fileformat" eq "flat" || "$fileformat" eq "symlink") {
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

        #print "$zipprog $zipmoveopt -u ../$jarfile.jar $args\n";

        # Handle posix cmdline limits
        while (length($args) > $maxCmdline) {
            #print "Exceeding POSIX cmdline limit: " . length($args) . "\n";
            my $subargs = substr($args, 0, $maxCmdline-1);
            my $pos = rindex($subargs, " ");
            $subargs = substr($args, 0, $pos);
            $args = substr($args, $pos);

            #print "$zipprog $zipmoveopt -u ../$jarfile.jar $subargs\n";
            #print "Length of subargs: " . length($subargs) . "\n";
            system("$zipprog $zipmoveopt -u ../$jarfile.jar $subargs") == 0 or
                $err = $? >> 8;
            zipErrorCheck($err,$lockfile);
        }
        #print "Length of args: " . length($args) . "\n";
        #print "$zipprog $zipmoveopt -u ../$jarfile.jar $args\n";
        system("$zipprog $zipmoveopt -u ../$jarfile.jar $args") == 0 or
            $err = $? >> 8;
        zipErrorCheck($err,$lockfile);
    }

    if (!($overrides eq "")) {
        my $err = 0; 
        print "+++ overriding $overrides\n";
          
        while (length($overrides) > $maxCmdline) {
            #print "Exceeding POSIX cmdline limit: " . length($overrides) . "\n";
            my $subargs = substr($overrides, 0, $maxCmdline-1);
            my $pos = rindex($subargs, " ");
            $subargs = substr($overrides, 0, $pos);
            $overrides = substr($overrides, $pos);

            #print "$zipprog $zipmoveopt ../$jarfile.jar $subargs\n";       
            #print "Length of subargs: " . length($subargs) . "\n";
            system("$zipprog $zipmoveopt ../$jarfile.jar $subargs") == 0 or
                $err = $? >> 8;
            zipErrorCheck($err,$lockfile);
        }
        #print "Length of args: " . length($overrides) . "\n";
        #print "$zipprog $zipmoveopt ../$jarfile.jar $overrides\n";
        system("$zipprog $zipmoveopt ../$jarfile.jar $overrides\n") == 0 or 
        $err = $? >> 8;
        zipErrorCheck($err,$lockfile);
    }
    mozUnlock($lockfile) if (!$nofilelocks);
    chdir($oldDir);
    #print "cd $oldDir\n";
}

sub _moz_rel2abs
{
    my ($path, $keep_file) = @_;
    $path = File::Spec->rel2abs($path, $objdir);
    my ($volume, $dirs, $file) = File::Spec->splitpath($path);
    my (@dirs) = reverse File::Spec->splitdir($dirs);
    my ($up) = File::Spec->updir();
    foreach (reverse 0 .. $#dirs) {
      splice(@dirs, $_, 2) if ($dirs[$_] eq $up);
    }
    $dirs = File::Spec->catdir(reverse @dirs);
    return File::Spec->catpath($volume, $dirs, $keep_file && $file);
}

sub _moz_abs2rel
{
    my ($target, $linkname) = @_;
    $target = _moz_rel2abs($target, 1);
    $linkname = _moz_rel2abs($linkname);
    return File::Spec->abs2rel($target, $linkname);
}

sub RegIt
{
    my ($chromeDir, $jarFileName, $chromeType, $pkgName) = @_;\
    chop($pkgName) if ($pkgName =~ m/\/$/);
    #print "RegIt:  $chromeDir, $jarFileName, $chromeType, $pkgName\n";

    my $line;
    if ($fileformat eq "flat" || $fileformat eq "symlink") {
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
    my $objPath;

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
        $objPath = "$objdir/$destFile";

        if (!-e $file) {
            if (!-e $objPath) {
                die "error: file '$file' doesn't exist";
            } else {
                $file = "$objPath";
            }
        }
        if (!-e $dir) {
            mkpath($dir, 0, 0775) || die "can't mkpath $dir: $!";
        }
        unlink $destPath;       # in case we had a symlink on unix
        if ($preproc) {
            my $preproc_file = $file;
            if ($^O eq 'cygwin' && $file =~ /^[a-zA-Z]:/) {
                # convert to a cygwin path
                $preproc_file =~ s|^([a-zA-Z]):|$cygwin_mountprefix/\1|;
            }
            if ($vms) {
                # use a temporary file otherwise cmd is too long for system()
                my $tmpFile = "$destPath.tmp";
                open(TMP, ">$tmpFile") || die("$tmpFile: $!");
                print(TMP "$^X $preprocessor $defines $preproc_file > $destPath");
                close(TMP);
                print "+++ preprocessing $preproc_file > $destPath\n";
                if (system("bash \"$tmpFile\"") != 0) {
                    die "Preprocessing of $file failed (VMS): ".($? >> 8);
                }
                unlink("$tmpFile") || die("$tmpFile: $!");
            } else {
                if (system("$^X $preprocessor $defines $preproc_file > $destPath") != 0) {
                    die "Preprocessing of $file failed: ".($? >> 8);
                }
            }
        } elsif ("$fileformat" eq "symlink") {
            $file = _moz_abs2rel($file, $destPath);
            symlink($file, $destPath) || die "symlink($file, $destPath) failed: $!";
            return 1;
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
                if (!foreignPlatformFile($jarfile)  && $autoreg &&
                    $dest =~ /([\w\d.\-\_\+]+)\/([\w\d.\-\_\\\/]+)contents.rdf/)
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
            } elsif (/^\*\+?\s+([\w\d.\-\_\\\/\+]+)\s*(\([\w\d.\-\_\\\/]+\))?$\s*/) {
                # preprocessed (always override)
                my $dest = $1;
                my $srcPath = defined($2) ? substr($2, 1, -1) : $2;
                EnsureFileInDir("$chromeDir/$jarfile", $baseFilesDir, $dest, $srcPath, 1, 1);
                $overrides = "$overrides$dest ";
                if (!foreignPlatformFile($jarfile)  && $autoreg && $dest =~ /([\w\d.\-\_\+]+)\/([\w\d.\-\_\\\/]+)contents.rdf/)
                {
                    my $chrome_type = $1;
                    my $pkg_name = $2;
                    RegIt($chromeDir, $jarfile, $chrome_type, $pkg_name);
                }
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
