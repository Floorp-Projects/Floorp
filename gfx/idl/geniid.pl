#!/usr/local/bin/perl

require "find.pl";

$uuid = 0x6f7652e0;
$format = "{ 0x%x,  0xee43, 0x11d1, \\\
 { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }";
$pattern = "--- IID GOES HERE ---";
$mydir = cwd();

sub replaceText {
    local ($oldname) = $_;
    local ($newname) = $_;
    local ($found) = 0;
    local ($tempname) = $oldname.'.orig';
    local ($replacement);

    if (-T $oldname && -s $oldname) {
        open(FILE, "<$oldname")
            || die "Unable to open $oldname\n";
        while (<FILE>) {
            if (/$pattern/) {
                $found = 1;
                last;
            }
        }
        close(FILE);

        if ($found) {
	    print "Setting IID for file: ", $oldname, "\n";
            rename($oldname, $tempname)
                || die "Unable to rename $oldname as $tempname";
            open(REPLACEFILE, ">$newname")
                || die "Unable to open $newname for writing\n";

            open(SEARCHFILE, "<$tempname")
                || die "Unable to open $tempname\n";

	    while (<SEARCHFILE>) { 
		if (/$pattern/) {
		    $replacement = sprintf($format, $uuid++);
		    s/$pattern/$replacement /g;
		}
		print REPLACEFILE;
	    }
            close(SEARCHFILE);
            close(REPLACEFILE);
            if (-z $newname) {
                die "$newname has zero size\n."
                    ."Restore manually from $tempname\n";
            } else {
                unlink($tempname);
            }

            warn "$name: Renaming as $newname\n" if $newname ne $oldname;

            $_ = $oldname;
            return;
        }
    }
    if ($newname ne $oldname) {
        warn "$name: Renaming as $newname\n";
        rename($oldname, $newname) || warn "Unable to rename $oldname\n";
    }
    $_ = $oldname;
}

eval 'exec /usr/local/bin/perl -S $0 ${1+"$@"}'
	if $running_under_some_shell;

# Traverse desired filesystems
$dont_use_nlink = 1;

if (!$ARGV[0]) {
    &find('.');
}
else {
    foreach $file (@ARGV) {
	chdir $mydir
        &find($file);
    }
}

exit;

sub wanted {
    /^nsIDOM.*\.h$/ &&
    &replaceText($name);
}
