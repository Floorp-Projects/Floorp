#!/usr/bin/perl

@cppfiles = find_files("../",'\.cpp$',0);
foreach ( @cppfiles ) { s/^\.\.\//\t\$\(MOZ_SRC\)\\ns\\cmd\\qtfe\\/; }

undef $/;
open(F,"< mozilla.mak") || die "Can't open mozilla.mak for reading";
$i = <F>;
close(F);
$files = join("\n",@cppfiles);
if ( ($i =~ s/#+\s+QtFE-files-here/$files/) ) {
    open(F,"> mozilla.mak") || die "Can't open mozilla.mak for writing";
    print F $i;
    close(F);
} else {
    die "Can't find tag (should be around line 1140): # QtFE-files-here";
}
exit 0;

#
# Finds files.
#
# Examples:
#   find_files("/usr","\.cpp$",1)   - finds .cpp files in /usr and below
#   find_files("/tmp","^#",0)	    - finds #* files in /tmp
#

sub find_files {
    my($dir,$match,$descend) = @_;
    my($file,$p,@files);
    local(*D);
    $dir =~ s=\\=/=g;
    ($dir eq "") && ($dir = ".");
    if ( opendir(D,$dir) ) {
	if ( $dir eq "." ) {
	    $dir = "";
	} else {
	    ($dir =~ /\/$/) || ($dir .= "/");
	}
	foreach $file ( readdir(D) ) {
	    next if ( $file  =~ /^\.\.?$/ );
	    $p = $dir . $file;
	    ($file =~ /$match/) && (push @files, $p);
	    if ( $descend && -d $p && ! -l $p ) {
		push @files, &find_files($p,$match,$descend);
	    }
	}
	closedir(D);
    }
    return @files;
}
