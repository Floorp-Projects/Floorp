#!perl

package			BuildList;
require			Exporter;

@ISA				= qw(Exporter);
@EXPORT			= qw(BuildMozilla DistMozilla PrepareDist);

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
				$dist_dir = ":mozilla:dist:client_debug:";
			}
		else
			{
				$D = "";
				$dist_dir = ":mozilla:dist:client:";
			}
		
			#
			# Build the appropriate target of each project
			#

		BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",										"Stubs");
		BuildProjectClean(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",									"Stubs");
		BuildProjectClean(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",			"Stubs");
		BuildProjectClean(":mozilla:cmd:macfe:projects:client:NavigatorStubs.mcp",		"Stubs");
		
		BuildProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",											"NSRuntime$D.shlb");
		MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:cmd:macfe:restext:StringLib.mcp",											"Strings$D.shlb");
		MakeAlias(":mozilla:cmd:macfe:restext:Strings$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp",							"MoreFiles$D.shlb");
		MakeAlias(":mozilla:lib:mac:MoreFiles:build:MoreFiles$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",												"NSPR20$D.shlb");
		MakeAlias(":mozilla:nsprpub:macbuild:NSPR20$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:dbm:macbuild:DBMPPC.mcp",															"DBM$D.shlb");
		MakeAlias(":mozilla:dbm:macbuild:DBM$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",					"MemAllocator$D.shlb");
		MakeAlias(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",												"NSStdLib$D.shlb");
		MakeAlias(":mozilla:lib:mac:NSStdLib:NSStdLib$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",													"xpcom$D.shlb");
		MakeAlias(":mozilla:xpcom:macbuild:xpcom$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:lib:mac:PowerPlant:PowerPlant.mcp",										"PowerPlant$D.shlb");		
		MakeAlias(":mozilla:lib:mac:PowerPlant:PowerPlant$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:modules:zlib:macbuild:zlib.mcp",												"zlib$D.shlb");
		MakeAlias(":mozilla:modules:zlib:macbuild:zlib$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:jpeg:macbuild:JPEG.mcp",																"JPEG$D.shlb");
		MakeAlias(":mozilla:jpeg:macbuild:JPEG$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:sun-java:stubs:macbuild:JavaStubs.mcp",								"JavaRuntime$D.shlb");
		MakeAlias(":mozilla:sun-java:stubs:macbuild:JavaRuntime$D.shlb", "$dist_dir");	
		
		BuildProject(":mozilla:js:jsj:macbuild:JSJ_PPC.mcp", 													"JSJ$D.o");
		
		BuildProject(":mozilla:js:macbuild:JavaScriptPPC.mcp",												"JavaScript$D.shlb");
		MakeAlias(":mozilla:js:macbuild:JavaScript$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:nav-java:stubs:macbuild:NavJavaStubs.mcp",							"NavJava$D.shlb");
		MakeAlias(":mozilla:nav-java:stubs:macbuild:NavJava$D.shlb", "$dist_dir");

		BuildProject(":mozilla:modules:rdf:macbuild:RDF.mcp", 												"RDF$D.shlb");
		MakeAlias(":mozilla:modules:rdf:macbuild:RDF$D.shlb", "$dist_dir");
	
		BuildProject(":mozilla:modules:xml:macbuild:XML.mcp",													"XML$D.shlb");
		MakeAlias(":mozilla:modules:xml:macbuild:XML$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:modules:schedulr:macbuild:Schedulr.mcp",								"Scheduler$D.shlb");
		MakeAlias(":mozilla:modules:schedulr:macbuild:Scheduler$D.shlb", "$dist_dir");
			
		BuildProject(":mozilla:build:mac:CustomLib:CustomLib.mcp",										"CustomLib$D.shlb");
		MakeAlias(":mozilla:build:mac:CustomLib:CustomLib$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:build:mac:CustomLib:CustomLib.mcp",										"CustomStaticLibs$D.o");
		BuildProject(":mozilla:modules:security:freenav:macbuild:NoSecurity.mcp",			"Security.o");
		BuildProject(":mozilla:modules:libfont:macbuild:FontBroker.mcp",							"FontBroker$D.o");
		BuildProject(":mozilla:lib:libmocha:macbuild:LibMocha.mcp",										"LibMocha$D.o");	
		BuildProject(":mozilla:network:macbuild:network.mcp",													"Network$D.o");
		
		if ( $main::MOZ_LITE == 0 )
			{
				BuildProject(":mozilla:cmd:macfe:Composer:build:Composer.mcp",						"Composer$D.o");
				
				# Build the appropriate resources target
				BuildProject(":mozilla:cmd:macfe:projects:client:Client.mcp", 						"Moz_Resources");
			}
		else
			{
				# Build a project with dummy targets to make stub libraries
				BuildProject("cmd:macfe:projects:dummies:MakeDummies.mcp",				"Composer$D.o");
				
				# Build the appropriate resources target
				BuildProject(":mozilla:cmd:macfe:projects:client:Client.mcp", 						"Nav_Resources");
			}
		
		BuildProject(":mozilla:cmd:macfe:projects:client:Client.mcp", 								"Client$D");
	}


sub PrepareDist()
	{
		MakeDirectoryPath(":mozilla:dist:");
		MakeDirectoryPath(":mozilla:dist:client:");
		MakeDirectoryPath(":mozilla:dist:client_debug:");
		MakeDirectoryPath(":mozilla:dist:client_stubs:");
	}


sub DistMozilla()
	{
		#INCLUDE
		InstallFromManifest(":mozilla:config:mac:export.mac",													":mozilla:dist:config:");
		InstallFromManifest(":mozilla:include:export.mac",														":mozilla:dist:include:");
		InstallFromManifest(":mozilla:cmd:macfe:pch:export.mac",											":mozilla:dist:include:");

		#MAC_COMMON
		InstallFromManifest(":mozilla:build:mac:export.mac",													":mozilla:dist:mac:common:");
		InstallFromManifest(":mozilla:lib:mac:NSStdLib:include:export.mac",						":mozilla:dist:mac:common:");
		InstallFromManifest(":mozilla:lib:mac:MacMemoryAllocator:include:export.mac",	":mozilla:dist:mac:common:");
		InstallFromManifest(":mozilla:lib:mac:Misc:export.mac",												":mozilla:dist:mac:common:");
		InstallFromManifest(":mozilla:lib:mac:MoreFiles:export.mac",									":mozilla:dist:mac:common:morefiles:");
		InstallFromManifest(":mozilla:cmd:macfe:MANIFEST",														":mozilla:dist:mac:macfe:");

		#NSPR
		InstallFromManifest(":mozilla:nsprpub:pr:include:MANIFEST",										":mozilla:dist:nspr:");
		InstallFromManifest(":mozilla:nsprpub:pr:src:md:mac:export.mac",							":mozilla:dist:nspr:mac:");
		InstallFromManifest(":mozilla:nsprpub:lib:ds:export.mac",											":mozilla:dist:nspr:");
		InstallFromManifest(":mozilla:nsprpub:lib:libc:include:export.mac",						":mozilla:dist:nspr:");
		InstallFromManifest(":mozilla:nsprpub:lib:msgc:include:export.mac",						":mozilla:dist:nspr:");
		
		#DBM
		InstallFromManifest(":mozilla:dbm:include:export.mac",												":mozilla:dist:dbm:");
		
		#LIBIMAGE
		InstallFromManifest(":mozilla:modules:libimg:png:export.mac",									":mozilla:dist:libimg:");
		InstallFromManifest(":mozilla:modules:libimg:src:export.mac",									":mozilla:dist:libimg:");
		InstallFromManifest(":mozilla:modules:libimg:public:export.mac",							":mozilla:dist:libimg:");
		
		#SECURITY_freenav
		InstallFromManifest(":mozilla:modules:security:freenav:export.mac",		":mozilla:dist:security:");
		
		#XPCOM
		InstallFromManifest(":mozilla:xpcom:src:export.mac",									":mozilla:dist:xpcom:");
		
		#ZLIB
		InstallFromManifest(":mozilla:modules:zlib:src:export.mac",						":mozilla:dist:zlib:");
				
		#JPEG
		InstallFromManifest(":mozilla:jpeg:export.mac",												":mozilla:dist:jpeg:");
		
		#JSJ
		InstallFromManifest(":mozilla:js:jsj:export.mac",											":mozilla:dist:jsj:");
		
		#JSDEBUG
		InstallFromManifest(":mozilla:js:jsd:export.mac",											":mozilla:dist:jsdebug:");
		
		#JS
		InstallFromManifest(":mozilla:js:src:export.mac",											":mozilla:dist:js:");
		
		#RDF
		InstallFromManifest(":mozilla:modules:rdf:include:export.mac",				":mozilla:dist:rdf:");
		
		#XML
		InstallFromManifest(":mozilla:modules:xml:glue:export.mac",						":mozilla:dist:xml:");
		InstallFromManifest(":mozilla:modules:xml:expat:xmlparse:export.mac",	":mozilla:dist:xml:");
		
		#LIBFONT
		InstallFromManifest(":mozilla:modules:libfont:MANIFEST",							":mozilla:dist:libfont:");
		InstallFromManifest(":mozilla:modules:libfont:src:export.mac",				":mozilla:dist:libfont:");
		
		#LDAP
		if ( $main::MOZ_LDAP )
			{
				InstallFromManifest(":mozilla:directory:c-sdk:ldap:include:MANIFEST", ":mozilla:dist:ldap:");
			}
			
		#SCHEDULER
		InstallFromManifest(":mozilla:modules:schedulr:public:export.mac",	":mozilla:dist:schedulr:");
		
		#NETWORK
		InstallFromManifest(":mozilla:network:cache:export.mac",						":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:client:export.mac",						":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:cnvts:export.mac",						":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:cstream:export.mac",					":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:main:export.mac",							":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:about:export.mac",		":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:certld:export.mac",	":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:dataurl:export.mac",	":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:file:export.mac",		":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:ftp:export.mac",			":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:gopher:export.mac",	":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:http:export.mac",		":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:js:export.mac",			":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:mailbox:export.mac",	":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:marimba:export.mac",	":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:nntp:export.mac",		":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:pop3:export.mac",		":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:remote:export.mac",	":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:smtp:export.mac",		":mozilla:dist:network:");
		
		#HTML_DIALOGS
		InstallFromManifest(":mozilla:lib:htmldlgs:export.mac",							":mozilla:dist:htmldlgs:");
		
		#LAYOUT
		InstallFromManifest(":mozilla:lib:layout:export.mac",								":mozilla:dist:layout:");
		
		#LAYERS
		InstallFromManifest(":mozilla:lib:liblayer:include:export.mac",			":mozilla:dist:layers:");
		
		#PARSE
		InstallFromManifest(":mozilla:lib:libparse:export.mac",							":mozilla:dist:libparse:");
		
		#STYLE
		InstallFromManifest(":mozilla:lib:libstyle:MANIFEST",								":mozilla:dist:libstyle:");
		
		#PLUGIN
		InstallFromManifest(":mozilla:lib:plugin:MANIFEST",									":mozilla:dist:plugin:");
		
		#LIBHOOK
		InstallFromManifest(":mozilla:modules:libhook:public:export.mac",		":mozilla:dist:libhook:");
		
		#LIBPREF
		InstallFromManifest(":mozilla:modules:libpref:public:export.mac",		":mozilla:dist:libpref:");
		
		#LIBREG
		InstallFromManifest(":mozilla:modules:libreg:include:export.mac",		":mozilla:dist:libreg:");
		
		#LIBUTIL
		InstallFromManifest(":mozilla:modules:libutil:public:export.mac",		":mozilla:dist:libutil:");
		
		#PROGRESS
		InstallFromManifest(":mozilla:modules:progress:public:export.mac",	":mozilla:dist:progress:");
		
		#SOFTUPDATE
		InstallFromManifest(":mozilla:modules:softupdt:include:export.mac",	":mozilla:dist:softupdate:");
		
		#EDTPLUG
		InstallFromManifest(":mozilla:modules:edtplug:include:MANIFEST", 		":mozilla:dist:edtplug:");

		#NAV_JAVA
		InstallFromManifest(":mozilla:nav-java:stubs:include:export.mac",		":mozilla:dist:nav-java:");
		InstallFromManifest(":mozilla:nav-java:stubs:macjri:export.mac",		":mozilla:dist:nav-java:");
		
		#SUN_JAVA
		InstallFromManifest(":mozilla:sun-java:stubs:include:export.mac",		":mozilla:dist:sun-java:");
		InstallFromManifest(":mozilla:sun-java:stubs:macjri:export.mac",		":mozilla:dist:sun-java:");
	}

1;


