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
		use File::Path;
		
sub BuildMozilla()
	{
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

		BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",							"Stubs");
		BuildProjectClean(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",						"Stubs");
		BuildProjectClean(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",			"Stubs");
		BuildProjectClean(":mozilla:cmd:macfe:projects:client:NavigatorStubs.mcp",			"Stubs");
		
		BuildProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",							"NSRuntime$D.shlb");
		MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:cmd:macfe:restext:StringLib.mcp",							"Strings$D.shlb");
		MakeAlias(":mozilla:cmd:macfe:restext:Strings$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp",					"MoreFiles$D.shlb");
		MakeAlias(":mozilla:lib:mac:MoreFiles:build:MoreFiles$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",								"NSPR20$D.shlb");
		MakeAlias(":mozilla:nsprpub:macbuild:NSPR20$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:dbm:macbuild:DBMPPC.mcp",									"DBM$D.shlb");
		MakeAlias(":mozilla:dbm:macbuild:DBM$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",				"MemAllocator$D.shlb");
		MakeAlias(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",								"NSStdLib$D.shlb");
		MakeAlias(":mozilla:lib:mac:NSStdLib:NSStdLib$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",								"xpcom$D.shlb");
		MakeAlias(":mozilla:xpcom:macbuild:xpcom$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:lib:mac:PowerPlant:PowerPlant.mcp",							"PowerPlant$D.shlb");		
		MakeAlias(":mozilla:lib:mac:PowerPlant:PowerPlant$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:modules:zlib:macbuild:zlib.mcp",								"zlib$D.shlb");
		MakeAlias(":mozilla:modules:zlib:macbuild:zlib$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:jpeg:macbuild:JPEG.mcp",										"JPEG$D.shlb");
		MakeAlias(":mozilla:jpeg:macbuild:JPEG$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:sun-java:stubs:macbuild:JavaStubs.mcp",						"JavaRuntime$D.shlb");
		MakeAlias(":mozilla:sun-java:stubs:macbuild:JavaRuntime$D.shlb", "$dist_dir");	
		
		BuildProject(":mozilla:js:jsj:macbuild:JSJ_PPC.mcp", 								"JSJ$D.o");
		
		BuildProject(":mozilla:js:macbuild:JavaScriptPPC.mcp",								"JavaScript$D.shlb");
		MakeAlias(":mozilla:js:macbuild:JavaScript$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:nav-java:stubs:macbuild:NavJavaStubs.mcp",					"NavJava$D.shlb");
		MakeAlias(":mozilla:nav-java:stubs:macbuild:NavJava$D.shlb", "$dist_dir");

		BuildProject(":mozilla:modules:rdf:macbuild:RDF.mcp", 								"RDF$D.shlb");
		MakeAlias(":mozilla:modules:rdf:macbuild:RDF$D.shlb", "$dist_dir");
	
		BuildProject(":mozilla:modules:xml:macbuild:XML.mcp",								"XML$D.shlb");
		MakeAlias(":mozilla:modules:xml:macbuild:XML$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:modules:schedulr:macbuild:Schedulr.mcp",						"Scheduler$D.shlb");
		MakeAlias(":mozilla:modules:schedulr:macbuild:Scheduler$D.shlb", "$dist_dir");
			
		BuildProject(":mozilla:build:mac:CustomLib:CustomLib.mcp",							"CustomLib$D.shlb");
		MakeAlias(":mozilla:build:mac:CustomLib:CustomLib$D.shlb", "$dist_dir");
		
		BuildProject(":mozilla:modules:security:freenav:macbuild:NoSecurity.mcp",			"Security.o");
		BuildProject(":mozilla:modules:libfont:macbuild:FontBroker.mcp",					"FontBroker$D.o");
		BuildProject(":mozilla:lib:libmocha:macbuild:LibMocha.mcp",							"LibMocha$D.o");	
		BuildProject(":mozilla:network:macbuild:network.mcp",								"Network$D.o");

		BuildProject(":mozilla:build:mac:CustomLib:CustomLib.mcp",							"CustomStaticLib$D.o");
		
		if ( $main::MOZ_MEDIUM == 1 || $main::MOZ_DARK == 1 )
		{
			BuildProject(":mozilla:cmd:macfe:Composer:build:Composer.mcp",					"Composer$D.o");

			if ( $main::MOZ_DARK == 1 )
			{
				BuildProject(":mozilla:lib:libmsg:macbuild:MsgLib.mcp",							"MsgLib$D.o");
				BuildProject(":mozilla:cmd:macfe:MailNews:build:MailNews.mcp",					"MailNews$D.o");
				BuildProject(":mozilla:directory:c-sdk:ldap:libraries:macintosh:LDAPClient.mcp","LDAPClient$D.o");
			}
			else
			{
				BuildProject(":mozilla:cmd:macfe:projects:dummies:MakeDummies.mcp",				"MsgLib$D.o");
				BuildProject(":mozilla:cmd:macfe:projects:dummies:MakeDummies.mcp",				"MailNews$D.o");
				BuildProject(":mozilla:cmd:macfe:projects:dummies:MakeDummies.mcp",				"LDAPClient$D.o");
			}

			# Build the appropriate resources target
			BuildProject(":mozilla:cmd:macfe:projects:client:Client.mcp", 					"Moz_Resources");
		}
		else
		{
			# Build a project with dummy targets to make stub libraries
			BuildProject("cmd:macfe:projects:dummies:MakeDummies.mcp",						"Composer$D.o");
			
			# Build the appropriate resources target
			BuildProject(":mozilla:cmd:macfe:projects:client:Client.mcp", 					"Nav_Resources");
		}
		
		BuildProject(":mozilla:cmd:macfe:projects:client:Client.mcp", 						"Client$D");
	}


sub DistMozilla()
	{
		mkpath([ ":mozilla:dist:", ":mozilla:dist:client:", ":mozilla:dist:client_debug:", ":mozilla:dist:client_stubs:" ]);

		#INCLUDE
		InstallFromManifest(":mozilla:config:mac:MANIFEST",								":mozilla:dist:config:");
		InstallFromManifest(":mozilla:include:MANIFEST",								":mozilla:dist:include:");
		InstallFromManifest(":mozilla:cmd:macfe:pch:MANIFEST",							":mozilla:dist:include:");

		#MAC_COMMON
		InstallFromManifest(":mozilla:build:mac:MANIFEST",								":mozilla:dist:mac:common:");
		InstallFromManifest(":mozilla:lib:mac:NSStdLib:include:MANIFEST",				":mozilla:dist:mac:common:");
		InstallFromManifest(":mozilla:lib:mac:MacMemoryAllocator:include:MANIFEST",		":mozilla:dist:mac:common:");
		InstallFromManifest(":mozilla:lib:mac:Misc:MANIFEST",							":mozilla:dist:mac:common:");
		InstallFromManifest(":mozilla:lib:mac:MoreFiles:MANIFEST",						":mozilla:dist:mac:common:morefiles:");
		InstallFromManifest(":mozilla:cmd:macfe:MANIFEST",								":mozilla:dist:mac:macfe:");

		#NSPR
		InstallFromManifest(":mozilla:nsprpub:pr:include:MANIFEST",						":mozilla:dist:nspr:");
		InstallFromManifest(":mozilla:nsprpub:pr:src:md:mac:MANIFEST",					":mozilla:dist:nspr:mac:");
		InstallFromManifest(":mozilla:nsprpub:lib:ds:MANIFEST",							":mozilla:dist:nspr:");
		InstallFromManifest(":mozilla:nsprpub:lib:libc:include:MANIFEST",				":mozilla:dist:nspr:");
		InstallFromManifest(":mozilla:nsprpub:lib:msgc:include:MANIFEST",				":mozilla:dist:nspr:");
		
		#DBM
		InstallFromManifest(":mozilla:dbm:include:MANIFEST",							":mozilla:dist:dbm:");
		
		#LIBIMAGE
		InstallFromManifest(":mozilla:modules:libimg:png:MANIFEST",						":mozilla:dist:libimg:");
		InstallFromManifest(":mozilla:modules:libimg:src:MANIFEST",						":mozilla:dist:libimg:");
		InstallFromManifest(":mozilla:modules:libimg:public:MANIFEST",					":mozilla:dist:libimg:");
		
		#SECURITY_freenav
		InstallFromManifest(":mozilla:modules:security:freenav:MANIFEST",				":mozilla:dist:security:");
		
		#XPCOM
		InstallFromManifest(":mozilla:xpcom:src:MANIFEST",								":mozilla:dist:xpcom:");
		
		#ZLIB
		InstallFromManifest(":mozilla:modules:zlib:src:MANIFEST",						":mozilla:dist:zlib:");
				
		#JPEG
		InstallFromManifest(":mozilla:jpeg:MANIFEST",									":mozilla:dist:jpeg:");
		
		#JSJ
		InstallFromManifest(":mozilla:js:jsj:MANIFEST",									":mozilla:dist:jsj:");
		
		#JSDEBUG
		InstallFromManifest(":mozilla:js:jsd:MANIFEST",									":mozilla:dist:jsdebug:");
		
		#JS
		InstallFromManifest(":mozilla:js:src:MANIFEST",									":mozilla:dist:js:");
		
		#RDF
		InstallFromManifest(":mozilla:modules:rdf:include:MANIFEST",					":mozilla:dist:rdf:");
		
		#XML
		InstallFromManifest(":mozilla:modules:xml:glue:MANIFEST",						":mozilla:dist:xml:");
		InstallFromManifest(":mozilla:modules:xml:expat:xmlparse:MANIFEST",				":mozilla:dist:xml:");
		
		#LIBFONT
		InstallFromManifest(":mozilla:modules:libfont:MANIFEST",						":mozilla:dist:libfont:");
		InstallFromManifest(":mozilla:modules:libfont:src:MANIFEST",					":mozilla:dist:libfont:");
		
		#LDAP
		if ( $main::MOZ_LDAP )
			{
				InstallFromManifest(":mozilla:directory:c-sdk:ldap:include:MANIFEST",	":mozilla:dist:ldap:");
			}
			
		#SCHEDULER
		InstallFromManifest(":mozilla:modules:schedulr:public:MANIFEST",				":mozilla:dist:schedulr:");
		
		#NETWORK
		InstallFromManifest(":mozilla:network:cache:MANIFEST",							":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:client:MANIFEST",							":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:cnvts:MANIFEST",							":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:cstream:MANIFEST",						":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:main:MANIFEST",							":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:mimetype:MANIFEST",						":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:util:MANIFEST",							":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:about:MANIFEST",					":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:certld:MANIFEST",				":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:dataurl:MANIFEST",				":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:file:MANIFEST",					":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:ftp:MANIFEST",					":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:gopher:MANIFEST",				":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:http:MANIFEST",					":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:js:MANIFEST",					":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:mailbox:MANIFEST",				":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:marimba:MANIFEST",				":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:nntp:MANIFEST",					":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:pop3:MANIFEST",					":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:remote:MANIFEST",				":mozilla:dist:network:");
		InstallFromManifest(":mozilla:network:protocol:smtp:MANIFEST",					":mozilla:dist:network:");
		
		#HTML_DIALOGS
		InstallFromManifest(":mozilla:lib:htmldlgs:MANIFEST",							":mozilla:dist:htmldlgs:");
		
		#LAYOUT
		InstallFromManifest(":mozilla:lib:layout:MANIFEST",								":mozilla:dist:layout:");
		
		#LAYERS
		InstallFromManifest(":mozilla:lib:liblayer:include:MANIFEST",					":mozilla:dist:layers:");
		
		#PARSE
		InstallFromManifest(":mozilla:lib:libparse:MANIFEST",							":mozilla:dist:libparse:");
		
		#STYLE
		InstallFromManifest(":mozilla:lib:libstyle:MANIFEST",							":mozilla:dist:libstyle:");
		
		#PLUGIN
		InstallFromManifest(":mozilla:lib:plugin:MANIFEST",								":mozilla:dist:plugin:");
		
		#LIBHOOK
		InstallFromManifest(":mozilla:modules:libhook:public:MANIFEST",					":mozilla:dist:libhook:");
		
		#LIBPREF
		InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST",					":mozilla:dist:libpref:");
		
		#LIBREG
		InstallFromManifest(":mozilla:modules:libreg:include:MANIFEST",					":mozilla:dist:libreg:");
		
		#LIBUTIL
		InstallFromManifest(":mozilla:modules:libutil:public:MANIFEST",					":mozilla:dist:libutil:");
		
		#PROGRESS
		InstallFromManifest(":mozilla:modules:progress:public:MANIFEST",				":mozilla:dist:progress:");
		
		#EDTPLUG
		InstallFromManifest(":mozilla:modules:edtplug:include:MANIFEST", 				":mozilla:dist:edtplug:");

		#NAV_JAVA
		InstallFromManifest(":mozilla:nav-java:stubs:include:MANIFEST",					":mozilla:dist:nav-java:");
		InstallFromManifest(":mozilla:nav-java:stubs:macjri:MANIFEST",					":mozilla:dist:nav-java:");
		
		#SUN_JAVA
		InstallFromManifest(":mozilla:sun-java:stubs:include:MANIFEST",					":mozilla:dist:sun-java:");
		InstallFromManifest(":mozilla:sun-java:stubs:macjri:MANIFEST",					":mozilla:dist:sun-java:");
	}

1;


