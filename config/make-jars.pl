#!/perl

# make-jars [-d <destPath>] < <jar.mn>

use Getopt::Std;
use Cwd;

sub RemoveAll
{
    my ($target) = @_;
    my $next = "";
    my $dpeth = 0;
    if( -e $target )
    {
        if ( -d $target ) {
            chdir($target);
            foreach ( <*> ) {
                $next = $_;
                if (-d $next ) {
                    RemoveAll($next);
                    rmdir($next);
                } else {
                    unlink($next);
                }
            }
            $depth = tr/$target// + 1;
#print "depth = $depth\n";
            for ($i = 1; $i < $depth; $i++) { chdir(".."); }
            rmdir($target);  #will this remove the whole directory tree or just then end dir?
        } else {
            unlink($target);
        }
    }
}

sub cleanup
{
    my (%removeList) = @_;
    my $target = "";
    print "+++ removing temp files\n";
    foreach $target (keys %removeList) {
        if ($target ) {
#            RemoveAll($target);
        }
    }
}

sub JarIt
{
    my ($jarfile, $args) = @_;
    system "zip -u $jarfile $args\n";
    my $cwd = cwd();
    print "+++ jarred $cwd => $jarfile\n";
}

sub MkDirs
{
    my ($path, $created) = @_; 

    if ($path =~ /([\w\d.\-]+)[\\\/](.*)/) {
        my $dir = $1;
        $path = $2;

        if (!-e $dir) {
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
            $created = 1;
#print "created = $created\n";
        } 
        chdir $dir;
        MkDirs($path, $created);
        chdir "..";
    }
    else {
        my $dir = $path;
        if ($dir eq "") { return $created; } #print "returning $created\n"; return $created; } 
        if (!-e $dir) {
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
            $created = 1;
#print "created = $created\n";
        }
    }
#print "returning $created\n";
    return $created;
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
    my ($destPath, $srcPath) = @_;

    if (!-e $destPath) {
print "need to copy $destPath\n";
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
            die "error: file '$file' doesn't exist\n";
        }
        if ( MkDirs($dir, 0) ) {
            $removeList{$dir} = 1;        
#print "adding dir $dir \n"; 
        }
        CopyFile($file, $destPath);

        if ( !$dir ) {
            $removeList{$destPath} = 1;
#print "adding file $destPath \n";
        }
        return 1;
    }
    return 0;
}

getopt("d:");

my $destPath = ".";
if (defined($opt_d)) {
    $destPath = $opt_d;
}

while (<>) {
    chomp;
  start: 
    if (/^([\w\d.\-\\\/]+)\:\s*$/) {
        my $jarfile = "$destPath/$1"; 

        my $args = "";
        %removeList = "";

        while (<>) {
            if (/^\s+([\w\d.\-\\\/]+)\s*(\([\w\d.\-\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = $2;

                if ( $srcPath ) {  
                    $srcPath = substr($srcPath,1,-1);
                }

                EnsureFileInDir($dest, $srcPath);
                $args = "$args$dest ";
            } elsif (/^\s*$/) {
                # end with blank line
                last;
            } else {
                JarIt($jarfile, $args);
                cleanup(%removeList);
                goto start;
            }
        }
        JarIt($jarfile, $args);
        cleanup(%removeList);
    } elsif (/^\s*\#.*$/) {
        # skip comments
    } elsif (/^\s*$/) {
        # skip blank lines
    } else {
        close;
        die "bad jar rule head at: $_";
    }
}
