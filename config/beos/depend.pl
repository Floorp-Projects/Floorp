#perl
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

# Build BeOS intermodule dependencies

$| = 1;

@SOLIST=<dist/bin/*.so dist/bin/component/*.so dist/BeOS*/bin/*.so /boot/develop/lib/x86/*.so>;

# load defined symbols
foreach $so (@SOLIST) {
	next if $so eq $name;
	if(open(SO, "nm -g $so |")) {
		$so =~ s/\/*[^\/]*\///g;
		$so =~ s/^lib//;
		$so =~ s/\.so$//;
		while($def = <SO>) {
			chop $def;
			$def =~ / [ACDT] (.*)$/;
			next unless $1;
			#if($defsyms{$1}) {
			#	print "$1 already defined in $defsyms{$1}\n";
			#}
			$defsyms{$1} = $so;
		}
		close(SO);
	}
}

mkdir "dependencies.beos", 0777;

foreach $name (@ARGV) {
	($libname = $name) =~ s/\/*[^\/]*\///g;
	$libname =~ s/^lib//;
	$libname =~ s/\.so$//;
	print "Processing $libname...\n";
	if(open(IN, "nm -gu $name |")) {
		@syms = <IN>;
		close IN;
		chop @syms;

		foreach $s (@syms) {
			$so = $defsyms{$s};
			$deps{$so}++ if $so && $so ne $libname;
			#print "$s not found\n" unless $so;
		}

		if(open(OUT, ">dependencies.beos/$libname.dependencies")) {
			foreach $key (sort(keys %deps)) {
				print OUT " $key";
			}
			print OUT "\n";
			close(OUT);
		}
	}
	undef %deps;
}
