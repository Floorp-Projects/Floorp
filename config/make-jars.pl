#!/perl

# make-jars [-d <destPath>] < <manifest.jr>

sub JarIt
{
    my ($jarfile, $args, $objDir) = @_;
    print "+++ jaring $jarfile\n";
    flush;
	chdir $objDir;
	$jarfile = "../$jarfile";
    system "zip -u $jarfile $args\n";
}

sub MkDirs
{
    my ($path) = @_;
    if ($path =~ /([\w\d.\-]+)[\\\/](.*)/) {
        my $dir = $1;
        $path = $2;
        if (!-e $dir) {
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
        }
        chdir $dir;
        MkDirs($path);
        chdir "..";
    }
    else {
        my $dir = $path;
        if (!-e $dir) {
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
        }
    }
}

sub CopyFile
{
    my ($from, $to) = @_;
    open(OUT, ">$to") || die "error: can't open '$to': $!";
    open(IN, "<$from") || die "error: can't open '$from': $!";
    while (<IN>) {
        print OUT $_;
    }
    close(IN) || die "error: can't close '$from': $!";
    close(OUT) || die "error: can't close '$to': $!";
}

sub EnsureFileInDir
{
    my ($destPath, $srcPath, $objDir) = @_;

    if (!-e $destPath) {
        $destPath =~ /(.*)[\\\/]([\w\d.\-]+)/;
        my $dir = $1;
        my $file = $2;

        if ($srcPath) {
            $file = $srcPath;
        }

        if (!-e $file) {
            die "error: file '$file' doesn't exist\n";
        }
        MkDirs("$objDir/$dir");
        CopyFile($file, "$objDir/$destPath");
        return 1;
    }
    return 0;
}

use Getopt::Std;

getopt("d:o:");

my $destPath = ".";
if (defined($opt_d)) {
    $destPath = $opt_d;
}

my $objDir;
if (defined($opt_o)) {
    $objDir = $opt_o;
}
else {
    die "Need to supply the -o <objdir> option.";
}

while (<>) {
    chomp;
  start: 
    if (/^([\w\d.\-\\\/]+)\:\s*$/) {
        my $jarfile = "$destPath/$1"; 

        my $args = "";
        while (<>) {
            if (/^\s+([\w\d.\-\\\/]+)\s*(\([\w\d.\-\\\/]+\))?$\s*/) {
				my $dest = $1;
                my $srcPath = $2;

                if ( $srcPath ) {  
                    $srcPath = substr($srcPath,1,-1);
                }

                EnsureFileInDir($dest, $srcPath, $objDir);
                $args = "$args$dest ";
            } elsif (/^\s*$/) {
                # end with blank line
                last;
            } else {
                goto start;
            }
        }
        JarIt($jarfile, $args, $objDir);

    } elsif (/^\s*\#.*$/) {
        # skip comments
    } elsif (/^\s*$/) {
        # skip blank lines
    } else {
        close;
        die "bad jar rule head at: $_";
    }
}
