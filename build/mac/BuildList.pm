#!perl

package			BuildList;
require			Exporter;

@ISA				= qw(Exporter);
@EXPORT			= qw(BuildMozilla);

=head1 NAME

BuildList - build the [ordered] set of projects needed to construct Mozilla

=head1 SYNOPSIS

...

=head1 COPYRIGHT

The contents of this file are subject to the Netscape Public License
Version 1.0 (the "NPL"); you may not use this file except in
compliance with the NPL.  You may obtain a copy of the NPL at
http://www.mozilla.org/NPL/

Software distributed under the NPL is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
for the specific language governing rights and limitations under the
NPL.

The Initial Developer of this code under the NPL is Netscape
Communications Corporation.  Portions created by Netscape are
Copyright (C) 1998 Netscape Communications Corporation.  All Rights
Reserved.

=cut


sub BuildMozilla()
	{
		use Moz;

		chdir(":::"); # assuming this script is in "...:mozilla:build:mac:", change dir to just above "mozilla"


		# Ideally, we would set the target name like so:
		# $target = $main::DEBUG ? "debug" : "optimized";
		# ...and all projects would have corresponding targets


		$D = $main::DEBUG ? " (Debug)" : "";	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
		$dist_dir = ":dist:";


			#
			# Build the appropriate target of each project
			#

		Moz::BuildProject(":lib:mac:NSStdLib:NSStdLib.mcp",              	"Stub Library");
		Moz::BuildProject(":lib:mac:MacMemoryAllocator:MemAllocator.mcp",	"Stub Library");
		Moz::BuildProject(":cmd:macfe:projects:client:Navigator.mcp",    	"Stub Library");


		Moz::BuildProject(":lib:mac:NSRuntime:NSRuntime.mcp");						symlink(":lib:mac:NSRuntime:NSRuntimePPCLib", "${dist_dir}NSRuntimePPCLib");
		Moz::BuildProject(":cmd:macfe:restext:NavStringLibPPC.mcp");			symlink(":cmd:macfe:restext:", "${dist_dir}");
		Moz::BuildProject(":lib:mac:MoreFiles:build:MoreFilesPPC.prj");		symlink(":lib:mac:MoreFiles:build:", "${dist_dir}");

		if ( $main::DEBUG )
			{
				Moz::BuildProject(":nsprpub:macbuild:NSPR20PPCDebug.mcp");		symlink(":nsprpub:macbuild:", "${dist_dir}");
				Moz::BuildProject(":dbm:macbuild:DBMPPCDebug.mcp");						symlink(":dbm:macbuild:", "${dist_dir}");
			}
		else
			{
				Moz::BuildProject(":nsprpub:macbuild:NSPR20PPC.mcp");					symlink(":nsprpub:macbuild:", "${dist_dir}");
				Moz::BuildProject(":dbm:macbuild:DBMPPC.mcp");								symlink(":dbm:macbuild:", "${dist_dir}");
			}

		Moz::BuildProject(":lib:mac:MacMemoryAllocator:MemAllocator.mcp",				"PPC Shared Library$D");	symlink(":lib:mac:MacMemoryAllocator:", "${dist_dir}");
		Moz::BuildProject(":lib:mac:NSStdLib:NSStdLib.mcp",              				"PPC Shared Library");		symlink(":lib:mac:NSStdLib:", "${dist_dir}");
		Moz::BuildProject(":modules:security:freenav:macbuild:NoSecurity.mcp",	"PPC Shared Library$D");	symlink(":modules:security:freenav:macbuild:", "${dist_dir}");

		if ( $main::DEBUG )
			{
				Moz::BuildProject(":xpcom:macbuild:xpcomPPCDebug.mcp");	symlink(":xpcom:macbuild:", "${dist_dir}");
			}
		else
			{
				Moz::BuildProject(":xpcom:macbuild:xpcomPPC.mcp");			symlink(":xpcom:macbuild:", "${dist_dir}");
			}

		Moz::BuildProject(":lib:mac:PowerPlant:PowerPlant.mcp");															symlink(":lib:mac:PowerPlant:", "${dist_dir}");
		Moz::BuildProject(":modules:zlib:macbuild:zlib.mcp",        "PPC Shared Library$D");	symlink(":modules:zlib:macbuild:", "${dist_dir}");
		Moz::BuildProject(":jpeg:macbuild:JPEG.mcp",                "PPC Shared Library$D");	symlink(":jpeg:macbuild:", "${dist_dir}");
		Moz::BuildProject(":sun-java:stubs:macbuild:JavaStubs.mcp",	"PPC Shared Library$D");	symlink(":sun-java:stubs:macbuild:", "${dist_dir}");

		if ( $main::DEBUG )
			{
				Moz::BuildProject(":js:jsj:macbuild:JSJ_PPCDebug.mcp");		symlink(":js:jsj:macbuild:", "${dist_dir}");
				Moz::BuildProject(":js:macbuild:JavaScriptPPCDebug.mcp");	symlink(":js:macbuild:", "${dist_dir}");
			}
		else
			{
				Moz::BuildProject(":js:jsj:macbuild:JSJ_PPC.mcp");				symlink(":js:jsj:macbuild:", "${dist_dir}");
				Moz::BuildProject(":js:macbuild:JavaScriptPPC.mcp");			symlink(":js:macbuild:", "${dist_dir}");
			}

		Moz::BuildProject(":nav-java:stubs:macbuild:NavJavaStubs.mcp",	"PPC Shared Library$D");	symlink(":nav-java:stubs:macbuild:", "${dist_dir}");


			# the following `if' can be fixed when we either rename the debug target of the RDF project, or of all the other projects
		if ( $main::DEBUG )
			{
				Moz::BuildProject(":modules:rdf:macbuild:RDF.mcp", "PPC Shared Library +D -LDAP");		symlink(":modules:rdf:macbuild:", "${dist_dir}");
			}
		else
			{
				Moz::BuildProject(":modules:rdf:macbuild:RDF.mcp", "PPC Shared Library -LDAP");				symlink(":modules:rdf:macbuild:", "${dist_dir}");
			}

		Moz::BuildProject(":modules:xml:macbuild:XML.mcp",            "PPC Shared Library$D");		symlink(":modules:xml:macbuild:", "${dist_dir}");
		Moz::BuildProject(":modules:libfont:macbuild:FontBroker.mcp",	"PPC Library$D");
		Moz::BuildProject(":modules:schedulr:macbuild:Schedulr.mcp",  "PPC Shared Library$D");		symlink(":modules:schedulr:macbuild:", "${dist_dir}");
		Moz::BuildProject(":network:macbuild:network.mcp",						"PPC Library (Debug Moz)");
		Moz::BuildProject(":cmd:macfe:Composer:build:Composer.mcp",   "PPC Library$D");
		Moz::BuildProject(":cmd:macfe:projects:client:Navigator.mcp", "Moz PPC App$D");
	}

1;


