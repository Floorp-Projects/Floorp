#!perl

package			BuildList;
require			Exporter;

@ISA				= qw(Exporter);
@EXPORT			= qw(BuildMozilla DistMozilla);

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

		use Moz;

sub BuildMozilla()
	{
		# chdir(":::"); # assuming this script is in "...:mozilla:build:mac:", change dir to just inside "mozilla"


		if ( $main::DEBUG )
			{
				$D = "Debug";
				$dist_dir = ":dist:client_debug:";
			}
		else
			{
				$D = "";
				$dist_dir = ":dist:client:";
			}

			#
			# Make the project that copies headers. The target is called "Stubs" so that
			# the AppleScript does a 'remove binaries' on the target first to guarantee
			# that it builds.
			#
		
		BuildProjectClean(":build:mac:MakeDist.mcp", "Stubs") unless $main::use_DistMozilla;
		
			#
			# Build the appropriate target of each project
			#

		BuildProjectClean(":lib:mac:NSStdLib:NSStdLib.mcp",										"Stubs");
		BuildProjectClean(":lib:mac:NSRuntime:NSRuntime.mcp",									"Stubs");
		BuildProjectClean(":lib:mac:MacMemoryAllocator:MemAllocator.mcp",			"Stubs");
		BuildProjectClean(":cmd:macfe:projects:client:NavigatorStubs.mcp",		"Stubs");
		
		BuildProject(":lib:mac:NSRuntime:NSRuntime.mcp",											"NSRuntime$D.shlb");
		MakeAlias(":lib:mac:NSRuntime:NSRuntime$D.shlb", "$dist_dir");
		
		BuildProject(":cmd:macfe:restext:StringLib.mcp",											"Strings$D.shlb");
		MakeAlias(":cmd:macfe:restext:Strings$D.shlb", "$dist_dir");
		
		BuildProject(":lib:mac:MoreFiles:build:MoreFilesPPC.mcp",							"MoreFiles$D.shlb");
		MakeAlias(":lib:mac:MoreFiles:build:MoreFiles$D.shlb", "$dist_dir");
		
		BuildProject(":nsprpub:macbuild:NSPR20PPC.mcp",												"NSPR20$D.shlb");
		MakeAlias(":nsprpub:macbuild:NSPR20$D.shlb", "$dist_dir");
		
		BuildProject(":dbm:macbuild:DBMPPC.mcp",															"DBM$D.shlb");
		MakeAlias(":dbm:macbuild:DBM$D.shlb", "$dist_dir");
		
		BuildProject(":lib:mac:MacMemoryAllocator:MemAllocator.mcp",					"MemAllocator$D.shlb");
		MakeAlias(":lib:mac:MacMemoryAllocator:MemAllocator$D.shlb", "$dist_dir");
		
		BuildProject(":lib:mac:NSStdLib:NSStdLib.mcp",												"NSStdLib$D.shlb");
		MakeAlias(":lib:mac:NSStdLib:NSStdLib$D.shlb", "$dist_dir");
		
		BuildProject(":modules:security:freenav:macbuild:NoSecurity.mcp",			"NoSecurity$D.shlb");
		MakeAlias(":modules:security:freenav:macbuild:NoSecurity$D.shlb", "$dist_dir");
		
		BuildProject(":xpcom:macbuild:xpcomPPC.mcp",													"xpcom$D.shlb");
		MakeAlias(":xpcom:macbuild:xpcom$D.shlb", "$dist_dir");
		
		BuildProject(":lib:mac:PowerPlant:PowerPlant.mcp",										"PowerPlant$D.shlb");		
		MakeAlias(":lib:mac:PowerPlant:PowerPlant$D.shlb", "$dist_dir");
		
		BuildProject(":modules:zlib:macbuild:zlib.mcp",												"zlib$D.shlb");
		MakeAlias(":modules:zlib:macbuild:zlib$D.shlb", "$dist_dir");
		
		BuildProject(":jpeg:macbuild:JPEG.mcp",																"JPEG$D.shlb");
		MakeAlias(":jpeg:macbuild:JPEG$D.shlb", "$dist_dir");
		
		BuildProject(":sun-java:stubs:macbuild:JavaStubs.mcp",								"JavaRuntime$D.shlb");
		MakeAlias(":sun-java:stubs:macbuild:JavaRuntime$D.shlb", "$dist_dir");	
		
		BuildProject(":js:jsj:macbuild:JSJ_PPC.mcp", 													"JSJ$D.o");
		
		BuildProject(":js:macbuild:JavaScriptPPC.mcp",												"JavaScript$D.shlb");
		MakeAlias(":js:macbuild:JavaScript$D.shlb", "$dist_dir");
		
		BuildProject(":nav-java:stubs:macbuild:NavJavaStubs.mcp",							"NavJava$D.shlb");
		MakeAlias(":nav-java:stubs:macbuild:NavJava$D.shlb", "$dist_dir");

		BuildProject(":modules:rdf:macbuild:RDF.mcp", 												"RDF$D.shlb");
		MakeAlias(":modules:rdf:macbuild:RDF$D.shlb", "$dist_dir");
	
		BuildProject(":modules:xml:macbuild:XML.mcp",													"XML$D.shlb");
		MakeAlias(":modules:xml:macbuild:XML$D.shlb", "$dist_dir");
		
		BuildProject(":modules:libfont:macbuild:FontBroker.mcp",							"FontBroker$D.o");
				
		BuildProject(":modules:schedulr:macbuild:Schedulr.mcp",								"Scheduler$D.shlb");
		MakeAlias(":modules:schedulr:macbuild:Scheduler$D.shlb", "$dist_dir");
		
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



sub DistMozilla()
	{
		#INCLUDE
		InstallFromManifest(":config:mac:export.mac",			":dist:config:");
		InstallFromManifest(":include:export.mac",				":dist:include:");
		InstallFromManifest(":cmd:macfe:pch:export.mac",	":dist:include:");

		#MAC_COMMON
		InstallFromManifest(":build:mac:export.mac",													":dist:mac:common:");
		InstallFromManifest(":lib:mac:NSStdLib:include:export.mac",						":dist:mac:common:");
		InstallFromManifest(":lib:mac:MacMemoryAllocator:include:export.mac",	":dist:mac:common:");
		InstallFromManifest(":lib:mac:Misc:export.mac",												":dist:mac:common:");
		InstallFromManifest(":lib:mac:MoreFiles:export.mac",									":dist:mac:common:morefiles:");
		InstallFromManifest(":cmd:macfe:MANIFEST",														":dist:mac:macfe:");

		#NSPR
		InstallFromManifest(":nsprpub:pr:include:MANIFEST",					":dist:nspr:");
		InstallFromManifest(":nsprpub:pr:src:md:mac:export.mac",		":dist:nspr:mac:");
		InstallFromManifest(":nsprpub:lib:ds:export.mac",						":dist:nspr:");
		InstallFromManifest(":nsprpub:lib:libc:include:export.mac",	":dist:nspr:");
		InstallFromManifest(":nsprpub:lib:msgc:include:export.mac",	":dist:nspr:");
		
		#DBM
		InstallFromManifest(":dbm:include:export.mac",	":dist:dbm:");
		
		#LIBIMAGE
		InstallFromManifest(":modules:libimg:png:export.mac",			":dist:libimg:");
		InstallFromManifest(":modules:libimg:src:export.mac",			":dist:libimg:");
		InstallFromManifest(":modules:libimg:public:export.mac",	":dist:libimg:");
		
		#SECURITY_freenav
		InstallFromManifest(":modules:security:freenav:export.mac",	":dist:security:");
		
		#XPCOM
		InstallFromManifest(":xpcom:src:export.mac",		":dist:xpcom:");
		
		#ZLIB
		InstallFromManifest(":modules:zlib:src:export.mac",	":dist:zlib:");
		
		#JPEG
		InstallFromManifest(":jpeg:export.mac",	":dist:jpeg:");
		
		#JSJ
		InstallFromManifest(":js:jsj:export.mac",	":dist:jsj:");
		
		#JSDEBUG
		InstallFromManifest(":js:jsd:export.mac",	":dist:jsdebug:");
		
		#JS
		InstallFromManifest(":js:src:export.mac",	":dist:js:");
		
		#RDF
		InstallFromManifest(":modules:rdf:include:export.mac",	":dist:rdf:");
		
		#XML
		InstallFromManifest(":modules:xml:glue:export.mac",						":dist:xml:");
		InstallFromManifest(":modules:xml:expat:xmlparse:export.mac",	":dist:xml:");
		
		#LIBFONT
		InstallFromManifest(":modules:libfont:MANIFEST",				":dist:libfont:");
		InstallFromManifest(":modules:libfont:src:export.mac",	":dist:libfont:");
		
		#SCHEDULER
		InstallFromManifest(":modules:schedulr:public:export.mac",	":dist:schedulr:");
		
		#NETWORK
		InstallFromManifest(":network:cache:export.mac",						":dist:network:");
		InstallFromManifest(":network:client:export.mac",						":dist:network:");
		InstallFromManifest(":network:cnvts:export.mac",						":dist:network:");
		InstallFromManifest(":network:cstream:export.mac",					":dist:network:");
		InstallFromManifest(":network:main:export.mac",							":dist:network:");
		InstallFromManifest(":network:protocol:about:export.mac",		":dist:network:");
		InstallFromManifest(":network:protocol:certld:export.mac",	":dist:network:");
		InstallFromManifest(":network:protocol:dataurl:export.mac",	":dist:network:");
		InstallFromManifest(":network:protocol:file:export.mac",		":dist:network:");
		InstallFromManifest(":network:protocol:ftp:export.mac",			":dist:network:");
		InstallFromManifest(":network:protocol:gopher:export.mac",	":dist:network:");
		InstallFromManifest(":network:protocol:http:export.mac",		":dist:network:");
		InstallFromManifest(":network:protocol:js:export.mac",			":dist:network:");
		InstallFromManifest(":network:protocol:mailbox:export.mac",	":dist:network:");
		InstallFromManifest(":network:protocol:marimba:export.mac",	":dist:network:");
		InstallFromManifest(":network:protocol:nntp:export.mac",		":dist:network:");
		InstallFromManifest(":network:protocol:pop3:export.mac",		":dist:network:");
		InstallFromManifest(":network:protocol:remote:export.mac",	":dist:network:");
		InstallFromManifest(":network:protocol:smtp:export.mac",		":dist:network:");
		
		#HTML_DIALOGS
		InstallFromManifest(":lib:htmldlgs:export.mac",	":dist:htmldlgs:");
		
		#LAYOUT
		InstallFromManifest(":lib:layout:export.mac",	":dist:layout:");
		
		#LAYERS
		InstallFromManifest(":lib:liblayer:include:export.mac",	":dist:layers:");
		
		#PARSE
		InstallFromManifest(":lib:libparse:export.mac",	":dist:libparse:");
		
		#STYLE
		InstallFromManifest(":lib:libstyle:export.mac",	":dist:libstyle:");
		
		#LIBHOOK
		InstallFromManifest(":modules:libhook:public:export.mac",	":dist:libhook:");
		
		#LIBPREF
		InstallFromManifest(":modules:libpref:public:export.mac",	":dist:libpref:");
		
		#LIBREG
		InstallFromManifest(":modules:libreg:include:export.mac",	":dist:libreg:");
		
		#LIBUTIL
		InstallFromManifest(":modules:libutil:public:export.mac",	":dist:libutil:");
		
		#PROGRESS
		InstallFromManifest(":modules:progress:public:export.mac",	":dist:progress:");
		
		#SOFTUPDATE
		InstallFromManifest(":modules:softupdt:include:export.mac",	":dist:softupdate:");
		
		#NAV_JAVA
		InstallFromManifest(":nav-java:stubs:macjri:export.mac",	":dist:nav-java:macjri:");
		InstallFromManifest(":nav-java:stubs:include:export.mac",	":dist:nav-java:");
		
		#SUN_JAVA
		InstallFromManifest(":sun-java:stubs:include:export.mac",	":dist:sun-java:include:");
		InstallFromManifest(":sun-java:stubs:macjri:export.mac",	":dist:sun-java:macjri:");
	}

1;


