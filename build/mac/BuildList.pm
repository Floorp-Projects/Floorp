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


		$D = $main::DEBUG ? "Debug" : "";	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
		
		if ( $main::DEBUG )
			{
				$dist_dir = ":dist:client_debug";
			}
		else
			{
				$dist_dir = ":dist:client";
			}

			#
			# Make the project that copies headers. The target is called "Stubs" so that
			# the AppleScript does a 'remove binaries' on the target first to guarantee
			# that it builds.
			#
		
		BuildProject(":build:mac:MakeDist.mcp",																"Stubs");
		
			#
			# Build the appropriate target of each project
			#

		BuildProject(":lib:mac:NSStdLib:NSStdLib.mcp",												"Stubs");
		BuildProject(":lib:mac:NSRuntime:NSRuntime.mcp",											"Stubs");
		BuildProject(":lib:mac:MacMemoryAllocator:MemAllocator.mcp",					"Stubs");
		BuildProject(":cmd:macfe:projects:client:NavigatorStubs.mcp",					"Stubs");
		
		BuildProject(":lib:mac:NSRuntime:NSRuntime.mcp",											"NSRuntime$D.shlb");
		MakeAlias(":lib:mac:NSRuntime:NSRuntime$D.shlb", "${dist_dir}:");
		
		BuildProject(":cmd:macfe:restext:StringLib.mcp",											"Strings$D.shlb");
		MakeAlias(":cmd:macfe:restext:Strings$D.shlb", "${dist_dir}:");
		
		BuildProject(":lib:mac:MoreFiles:build:MoreFilesPPC.mcp",							"MoreFiles$D.shlb");
		MakeAlias(":lib:mac:MoreFiles:build:MoreFiles$D.shlb", "${dist_dir}:");
		
		BuildProject(":nsprpub:macbuild:NSPR20PPC.mcp",												"NSPR20$D.shlb");
		MakeAlias(":nsprpub:macbuild:NSPR20$D.shlb", "${dist_dir}:");
		
		BuildProject(":dbm:macbuild:DBMPPC.mcp",															"DBM$D.shlb");
		MakeAlias(":dbm:macbuild:DBM$D.shlb", "${dist_dir}:");
		
		BuildProject(":lib:mac:MacMemoryAllocator:MemAllocator.mcp",					"MemAllocator$D.shlb");
		MakeAlias(":lib:mac:MacMemoryAllocator:MemAllocator$D.shlb", "${dist_dir}:");
		
		BuildProject(":lib:mac:NSStdLib:NSStdLib.mcp",												"NSStdLib$D.shlb");
		MakeAlias(":lib:mac:NSStdLib:NSStdLib$D.shlb", "${dist_dir}:");
		
		BuildProject(":modules:security:freenav:macbuild:NoSecurity.mcp",			"NoSecurity$D.shlb");
		MakeAlias(":modules:security:freenav:macbuild:NoSecurity$D.shlb", "${dist_dir}:");
		
		BuildProject(":xpcom:macbuild:xpcomPPC.mcp",													"xpcom$D.shlb");
		MakeAlias(":xpcom:macbuild:xpcom$D.shlb", "${dist_dir}:");
		
		BuildProject(":lib:mac:PowerPlant:PowerPlant.mcp",										"PowerPlant$D.shlb");		
		MakeAlias(":lib:mac:PowerPlant:PowerPlant$D.shlb", "${dist_dir}:");
		
		BuildProject(":modules:zlib:macbuild:zlib.mcp",												"zlib$D.shlb");
		MakeAlias(":modules:zlib:macbuild:zlib$D.shlb", "${dist_dir}:");
		
		BuildProject(":jpeg:macbuild:JPEG.mcp",																"JPEG$D.shlb");
		MakeAlias(":jpeg:macbuild:JPEG$D.shlb", "${dist_dir}:");
		
		BuildProject(":sun-java:stubs:macbuild:JavaStubs.mcp",								"JavaRuntime$D.shlb");
		MakeAlias(":sun-java:stubs:macbuild:JavaRuntime$D.shlb", "${dist_dir}:");	
		
		BuildProject(":js:jsj:macbuild:JSJ_PPC.mcp", 												"JSJ$D.o");
		
		BuildProject(":js:macbuild:JavaScriptPPC.mcp",												"JavaScript$D.shlb");
		MakeAlias(":js:macbuild:JavaScript$D.shlb", "${dist_dir}:");
		
		BuildProject(":nav-java:stubs:macbuild:NavJavaStubs.mcp",							"NavJava$D.shlb");
		MakeAlias(":nav-java:stubs:macbuild:NavJava$D.shlb", "${dist_dir}:");

		BuildProject(":modules:rdf:macbuild:RDF.mcp", 												"RDF$D.shlb");
		MakeAlias(":modules:rdf:macbuild:RDF$D.shlb", "${dist_dir}:");
	
		BuildProject(":modules:xml:macbuild:XML.mcp",													"XML$D.shlb");
		MakeAlias(":modules:xml:macbuild:XML$D.shlb", "${dist_dir}:");
		
		BuildProject(":modules:libfont:macbuild:FontBroker.mcp",							"FontBroker$D.o");
				
		BuildProject(":modules:schedulr:macbuild:Schedulr.mcp",								"Scheduler$D.shlb");
		MakeAlias(":modules:schedulr:macbuild:Scheduler$D.shlb", "${dist_dir}:");
		
		BuildProject(":network:macbuild:network.mcp",													"Network$D.o");
		
		if ( $main::MOZ_LITE == 0 )
			{
				BuildProject(":cmd:macfe:Composer:build:Composer.mcp",						"Composer$D.o");
				
				# Build the appropriate resources target
				BuildProject(":cmd:macfe:projects:client:Client.mcp", 						"Moz_Resources");
			}
		else
			{
				# Build a project with dummy targets to make stub libraries
				BuildProject("cmd:macfe:projects:dummies:MakeDummies.mcp",				"Composer$D.o");
				
				# Build the appropriate resources target
				BuildProject(":cmd:macfe:projects:client:Client.mcp", 						"Nav_Resources");
			}
		
		BuildProject(":cmd:macfe:projects:client:Client.mcp", 								"Client$D");
	}

1;


