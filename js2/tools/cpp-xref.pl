#!/bin/perl
use strict;

my $dir = shift || "./";
my @files = get_cpp_files ($dir);
my $file;
my %results;
my $verbose = 0;

foreach $file (@files) {
    process_file ($file);
}

foreach $file (@files) {
    my $c = $results{$file};
    my @keys = sort (keys (%{$c}));
    my $key;

    print "$file:\n";

    if ($verbose) {
        foreach $key (@keys) {
            if ($key !~ /^HASH/) {
                print "\t$key: $c->{$key}\n";
            }
        }
    }

    if ($c->{"hard_tabs"}) {
        print "\tHard Tabs: ", $c->{"hard_tabs"}, "\n";
    }
    
    if ($c->{"sys_includes"}) {
        my $incs = join (", ", @{$c->{"sys_includes"}});
        print "\tSystem Includes: $incs\n";
    }

    if ($c->{"local_includes"}) {
        my $incs = join (", ", @{$c->{"local_includes"}});
        print "\tLocal Includes: $incs\n";
    }
    
    print "\n";
}

sub process_file {
    my ($filename) = @_;
    my %c = {};
    open (FILE, $filename);

    while (my $line = <FILE>) {
        if ($line =~ /\t/) {
            if ($c{"hard_tabs"}) {
                ++$c{"hard_tabs"};
            } else {
                $c{"hard_tabs"} = 1;
            }
        }
        if ($line =~ /(\/\*|\/\/)\s*-\*-\s*Mode: (\S+)/i) {
            # it's an emacs mode line
            $c{"mode"} = $2;
            if ($line =~ /tab-width: (\d+)/i) {
                $c{"tab_width"} = $1;
            }
            if ($line =~ /indent-tabs-mode: (\S+)/i) {
                $c{"tab_mode"} = $1;
            }
            if ($line =~ /c-basic-offset: (\d+)/i) {
                $c{"basic_offset"} = $1;
            }
        } elsif ($line =~
            /(\*|\/\/)\s*The contents of this file are subject to the (\S+)/i) {
            # first line of license
            $c{"has_license"} = 1;
            $c{"license_comment"} = ($1 eq "*") ? "/**/" : "//";
            $c{"license_type"} = (lc($2) eq "netscape") ? "NPL" : "MPL";
        } elsif ($line =~ /The Original Code is (.*)/i) {
            # original code line
            $c{"o_code"} = $1;
        } elsif ($line =~ /Alternatively, the contents of this file may be used under/i) {
            $c{"license_dual"} = 1;

        } elsif ($line =~ /^\#include ([\"<])(\S+)[>\"]/i) {
            # include directive
            if ($1 eq "\"") {
                push (@{$c{"local_includes"}}, $2);
            } else {
                push (@{$c{"sys_includes"}}, $2);
            }
        }
    }

    $results{$filename} = \%c;
}
    
#
# given a directory, return an array of all subdirectories
#
sub get_subdirs {
    my ($dir)  = @_;
    my @subdirs;
    
    if (!($dir =~ /\/$/)) {
        $dir = $dir . "/";
    }
        
    opendir (DIR, $dir) || die ("couldn't open directory $dir: $!");
    my @contents = readdir(DIR);
    closedir(DIR);
    
    foreach (@contents) {
        if ((-d ($dir . $_)) && ($_ ne 'CVS') && ($_ ne '.') && ($_ ne '..')) {
            @subdirs[$#subdirs + 1] = $_;
        }
    }

    return @subdirs;
}

#
# given a directory, return an array of all the .h and .cpp files.
#
sub get_cpp_files {
    my ($subdir) = @_;
    my (@file_array, @files);
    
    opendir (SUBDIR, $subdir) || die ("couldn't open directory $subdir: $!");
    @files = readdir(SUBDIR);
    closedir(SUBDIR);
    
    foreach (@files) {
        if ($_ =~ /\.h$|\.cpp$/) {
            push (@file_array, $_);
        }
    }
    
    return sort(@file_array);
}
