#!perl

package			BuildMozilla;
require			Exporter;

@ISA				= qw(Exporter);
@EXPORT			= qw(BuildMozilla);

=head1 NAME

BuildMozilla - build the [ordered] set of projects needed to construct Mozilla

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

		chdir("::::"); # assuming this script is in "...:mozilla:build:mac:", change dir to just above "mozilla"


		# Ideally, we would set the target name like so:
		# $target = $main::DEBUG ? "debug" : "optimized";
		# ...and all projects would have corresponding targets


		$D = $main::DEBUG ? " (Debug)" : "";	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project


			#
			# Build the appropriate target of each project
			#

		Moz::BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              				"Stub Library");
		Moz::BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",				"Stub Library");
		Moz::BuildProject(":mozilla:cmd:macfe:projects:client:Navigator.mcp",    				"Stub Library");

		Moz::BuildProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp");
		Moz::BuildProject(":mozilla:cmd:macfe:restext:NavStringLibPPC.mcp");
		Moz::BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.prj");

		if ( $main::DEBUG )
			{
				Moz::BuildProject(":mozilla:nsprpub:macbuild:NSPR20PPCDebug.mcp");
				Moz::BuildProject(":mozilla:dbm:macbuild:DBMPPCDebug.mcp");
			}
		else
			{
				Moz::BuildProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp");
				Moz::BuildProject(":mozilla:dbm:macbuild:DBMPPC.mcp");
			}

		Moz::BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",				"PPC Shared Library$D");
		Moz::BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              				"PPC Shared Library");
		Moz::BuildProject(":mozilla:modules:security:freenav:macbuild:NoSecurity.mcp",	"PPC Shared Library$D");

		if ( $main::DEBUG )
			{
				Moz::BuildProject(":mozilla:xpcom:macbuild:xpcomPPCDebug.mcp");
			}
		else
			{
				Moz::BuildProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp");
			}

		Moz::BuildProject(":mozilla:lib:mac:PowerPlant:PowerPlant.mcp");
		Moz::BuildProject(":mozilla:modules:zlib:macbuild:zlib.mcp",            			  "PPC Shared Library$D");
		Moz::BuildProject(":mozilla:jpeg:macbuild:JPEG.mcp",                    			  "PPC Shared Library$D");
		Moz::BuildProject(":mozilla:sun-java:stubs:macbuild:JavaStubs.mcp",      				"PPC Shared Library$D");

		if ( $main::DEBUG )
			{
				Moz::BuildProject(":mozilla:js:jsj:macbuild:JSJ_PPCDebug.mcp");
				Moz::BuildProject(":mozilla:js:macbuild:JavaScriptPPCDebug.mcp");
			}
		else
			{
				Moz::BuildProject(":mozilla:js:jsj:macbuild:JSJ_PPC.mcp");
				Moz::BuildProject(":mozilla:js:macbuild:JavaScriptPPC.mcp");
			}

		Moz::BuildProject(":mozilla:nav-java:stubs:macbuild:NavJavaStubs.mcp",   				"PPC Shared Library$D");


		# the following `if' can be fixed when we either rename the debug target of the RDF project, or of all the other projects

		if ( $main::DEBUG )
			{
				Moz::BuildProject(":mozilla:modules:rdf:macbuild:RDF.mcp",               		"PPC Shared Library +D -LDAP");
			}
		else
			{
				Moz::BuildProject(":mozilla:modules:rdf:macbuild:RDF.mcp",               		"PPC Shared Library -LDAP");
			}

		Moz::BuildProject(":mozilla:modules:xml:macbuild:XML.mcp",               				"PPC Shared Library$D");
		Moz::BuildProject(":mozilla:modules:libfont:macbuild:FontBroker.mcp",    				"PPC Library$D");
		Moz::BuildProject(":mozilla:modules:schedulr:macbuild:Schedulr.mcp",     				"PPC Shared Library$D");
		Moz::BuildProject(":mozilla:network:macbuild:network.mcp",											"PPC Library (Debug Moz)");
		Moz::BuildProject(":mozilla:cmd:macfe:Composer:build:Composer.mcp",   					"PPC Library$D");
		Moz::BuildProject(":mozilla:cmd:macfe:projects:client:Navigator.mcp",   				"Moz PPC App$D");
	}

1;


