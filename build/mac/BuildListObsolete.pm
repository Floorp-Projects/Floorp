#!perl

package			BuildListObsolete;
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

		chdir(":::"); # assuming this script is in "...:mozilla:build:mac:", change dir to just inside "mozilla"


		if ( $main::DEBUG )
			{
				$D = " (Debug)";
				$LibD = "Debug";
				$dist_dir = ":dist:client_debug:";
			}
		else
			{
				$D = "";
				$LibD = "";
				$dist_dir = ":dist:client:";
			}

			#
			# Build the appropriate target of each project
			#

		BuildProjectClean(":lib:mac:NSStdLib:NSStdLib.mcp",										"Stub Library");
		BuildProjectClean(":lib:mac:MacMemoryAllocator:MemAllocator.mcp",			"Stub Library");
		BuildProjectClean(":cmd:macfe:projects:client:NavigatorStubs.mcp",		"Stub Library");
		
		BuildProject(":lib:mac:NSRuntime:NSRuntime.mcp");
		MakeAlias(":lib:mac:NSRuntime:NSRuntimePPCLib", "$dist_dir");
		
		BuildProject(":cmd:macfe:restext:NavStringLibPPC.mcp");
		MakeAlias(":cmd:macfe:restext:StringsPPCLib", "$dist_dir");
		
		BuildProject(":lib:mac:MoreFiles:build:MoreFilesPPC.prj");
		MakeAlias(":lib:mac:MoreFiles:build:MoreFilesPPC.lib", "$dist_dir");
		
		BuildProject(":nsprpub:macbuild:NSPR20PPC".$LibD.".mcp");
		MakeAlias(":nsprpub:macbuild:NSPR20PPC".$LibD."Lib", "$dist_dir");
		
		BuildProject(":dbm:macbuild:DBMPPC".$LibD.".mcp");
		MakeAlias(":dbm:macbuild:DBMPPC".$LibD."Lib", "${dist_dir}");

		BuildProject(":lib:mac:MacMemoryAllocator:MemAllocator.mcp",					"PPC Shared Library$D");
		MakeAlias(":lib:mac:MacMemoryAllocator:MemAllocatorPPC".$LibD."Lib", "$dist_dir");
			
		BuildProject(":lib:mac:NSStdLib:NSStdLib.mcp",												"PPC Shared Library");
		MakeAlias(":lib:mac:NSStdLib:NSStdLibPPCLib", "$dist_dir");
		
		BuildProject(":modules:security:freenav:macbuild:NoSecurity.mcp",			"PPC Shared Library$D");
		MakeAlias(":modules:security:freenav:macbuild:NoSecurity".$LibD."Lib", "$dist_dir");
		
		BuildProject(":xpcom:macbuild:xpcomPPC$Lib.mcp");
		MakeAlias(":xpcom:macbuild:xpcomPPC".$LibD."Lib", "$dist_dir");
		
		BuildProject(":lib:mac:PowerPlant:PowerPlant.mcp");		
		MakeAlias(":lib:mac:PowerPlant:PowerPlantPPC".$LibD."Lib", "$dist_dir");
		
		BuildProject(":modules:zlib:macbuild:zlib.mcp",												"PPC Shared Library$D");
		MakeAlias(":modules:zlib:macbuild:zlib".$LibD."Lib", "$dist_dir");
		
		BuildProject(":jpeg:macbuild:JPEG.mcp",																"PPC Shared Library$D");
		MakeAlias(":jpeg:macbuild:JPEGPPC".$LibD."Lib", "$dist_dir");
		
		BuildProject(":sun-java:stubs:macbuild:JavaStubs.mcp",								"PPC Shared Library$D");
		MakeAlias(":sun-java:stubs:macbuild:JavaRuntimePPC".$LibD."Lib", "$dist_dir");	
		
		BuildProject(":js:jsj:macbuild:JSJ_PPC".$LibD.".mcp");
		
		BuildProject(":js:macbuild:JavaScriptPPC".$LibD.".mcp");
		MakeAlias(":js:macbuild:JavaScriptPPC".$LibD."Lib", "$dist_dir");
		
		BuildProject(":nav-java:stubs:macbuild:NavJavaStubs.mcp",							"PPC Shared Library$D");
		MakeAlias(":nav-java:stubs:macbuild:NavJavaDebug".$LibD."Lib", "$dist_dir");

		if ( $main::DEBUG )
			{
				BuildProject(":modules:rdf:macbuild:RDF.mcp", 										"PPC Shared Library +D -LDAP");
				MakeAlias(":modules:rdf:macbuild:RDFPPCDebugLib", "${dist_dir}");
			}
		else
			{
				Moz::BuildProject(":modules:rdf:macbuild:RDF.mcp", 								"PPC Shared Library -LDAP");
				MakeAlias(":modules:rdf:macbuild:RDFPPCLib", "${dist_dir}");
			}

		BuildProject(":modules:xml:macbuild:XML.mcp",													"PPC Shared Library$D");
		MakeAlias(":modules:xml:macbuild:XMLPPC".$LibD."Lib", "$dist_dir");
		
		BuildProject(":modules:libfont:macbuild:FontBroker.mcp",							"PPC Library$D");
				
		BuildProject(":modules:schedulr:macbuild:Schedulr.mcp",								"PPC Shared Library$D");
		MakeAlias(":modules:schedulr:macbuild:SchedulerPPC".$LibD."Lib", "$dist_dir");
		
		BuildProject(":network:macbuild:network.mcp",													"PPC Library (Debug Moz)");
		
		if ( $main::MOZ_LITE == 0 )
			{
				BuildProject(":cmd:macfe:Composer:build:Composer.mcp",						"PPC Library$D");
			}
		
		BuildProject(":cmd:macfe:projects:client:Navigator.mcp", 							"Moz PPC App$D");
	}

1;


