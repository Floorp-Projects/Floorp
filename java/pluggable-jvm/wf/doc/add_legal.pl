#!/usr/bin/perl
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is The Waterfall Java Plugin Module
#  
# The Initial Developer of the Original Code is Sun Microsystems Inc
# Portions created by Sun Microsystems Inc are Copyright (C) 2001
# All Rights Reserved.
# 
# $Id: add_legal.pl,v 1.1 2001/07/12 20:25:38 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 

require "rules.pl";

$relpath = "";
sub get_type
{
    my ($name) = @_;
    foreach (keys %mapping) 
    {
	return $mapping{$_} if ($name =~ /$_/);
    }
    return undef;    
}

sub handle_file
{    
    my ($file, $name) = @_;
    my $type = get_type($name);
    open(FILE, $file) || die "cannot read $file: $!"; 
    #print "name=$name type=$type\n";
    my @lines = <FILE>;    
    close(FILE);
    my @newlines;
    if ($type) {	
	my ($pre, $com1, $com2, $com3) = 
	    ($prefixes{$type}, $comments_start{$type}, 
	     $comments_middle{$type}, $comments_end{$type});	
	if (!$com1 || !com2 || !com3) {
	    print "not defined comments for type $type - skipping...\n";
	    @newlines = @lines;
	} else {	    
	    @newlines = proceed($file, $type, @lines);
	}
    } else {
	 print "skipped $file...\n";
	 @newlines = @lines;
    }	        
    my $newname = "$newtree/$relpath$name";
    open(FILE, ">$newname") || die "cannot write to $newname: $!"; ;
    foreach (@newlines) {
	print FILE $_;
    }
    close(FILE);
}

sub handle_dir
{
    my ($dir, $short) = @_;
    my $oldrelpath = $relpath;
    $relpath .= $short."/" if ($short);
    opendir(DIR, $dir) || die "can't opendir $dir: $!";
    my $newdir = "$newtree/$relpath";
    mkdir $newdir unless (-d $newdir);
    # it will remove all starting with dot entries
    my @entries =  grep { !/^\.$/ && !/^\.\.$/} readdir(DIR);
    closedir DIR;
    my @filez = grep { -f "$dir/$_"} @entries;
    foreach (@filez) { handle_file("$dir/$_", $_); }
    my @dirz = grep { -d "$dir/$_" && !$ignore_dirs{$_}} @entries;
    foreach (@dirz) { 
	print "Handling $relpath$_...\n";	
	handle_dir("$dir/$_", $_);
    }
    $relpath = $oldrelpath;
}


open(LIC, $license) || die "can't read license file $license: $!";
@lic = <LIC>;
close (LIC);

handle_dir($oldtree, "");






