#!perl -w
package			NGLayoutBuildList;

require 5.004;
require Exporter;

use strict;
use vars qw( @ISA @EXPORT );

# perl includes
use Mac::StandardFile;
use Mac::Processes;
use Mac::Events;
use Cwd;
use File::Path;

# homegrown
use Moz;
use MacCVS;
use MANIFESTO;

@ISA			= qw(Exporter);
@EXPORT		= qw(Checkout BuildDist BuildProjects BuildCommonProjects BuildLayoutProjects);

# NGLayoutBuildList builds the nglayout project
# it is configured by setting the following variables in the caller:
# Usage:
# caller variables that affect behaviour:
# DEBUG		: 1 if we are building a debug version
# 3-part build process: checkout, dist, and build_projects
# Hack alert:
# NGLayout defines are located in :mozilla:config:mac:NGLayoutConfigInclude.h
# An alias "MacConfigInclude.h" to this file is created inside dist:config
# Note that the name of alias is different than the name of the file. This
# is to trick CW into including NGLayout defines 


#//--------------------------------------------------------------------------------------------------
#// Utility routines
#//--------------------------------------------------------------------------------------------------

# pickWithMemoryFile stores the information about the user pick inside
# the file $session_storage
sub _pickWithMemoryFile($)
{
	my ($sessionStorage) = @_;
	my $cvsfile;

	if (( -e $sessionStorage) &&
		 open( SESSIONFILE, $sessionStorage ))
	{
	# Read in the path if available
	  $cvsfile = <SESSIONFILE>;
	  chomp $cvsfile;
		close SESSIONFILE;
		if ( ! -e $cvsfile )
		{
			print STDERR "$cvsfile has disappeared\n";
			undef $cvsfile;
		}
	}
	unless (defined ($cvsfile))
	{
		# make sure that MacPerl is a front process
		ActivateApplication('McPL');
		MacPerl::Answer("Could not find your MacCVS session file. Please choose one", "OK");
		
		# prompt user for the file name, and store it
		my $macFile = StandardGetFile( 0, "McvD");	
		if ( $macFile->sfGood() )
		{
			$cvsfile = $macFile->sfFile();
		# save the choice if we can
			if ( open (SESSIONFILE, ">" . $sessionStorage))
			{
				printf SESSIONFILE $cvsfile, "\n";
				close SESSIONFILE;
			}
			else
			{
				print STDERR "Could not open storage file\n";
			}
		}
	}
	return $cvsfile;
}

# assert that we are in the correct directory for the build
sub _assertRightDirectory()
{
	unless (-e ":mozilla")
	{
		my($dir) = cwd();
		print STDERR "NGLayoutBuildList called from incorrect directory: $dir";
	} 
}

sub _getDistDirectory()
{
	return $main::DEBUG ? ":mozilla:dist:viewer_debug:" : ":mozilla:dist:viewer:";
}


#//--------------------------------------------------------------------------------------------------
#// Checkout everything
#//--------------------------------------------------------------------------------------------------

sub Checkout()
{
	# give application activation a chance to happen
	WaitNextEvent();
	WaitNextEvent();
	WaitNextEvent();
	
	_assertRightDirectory();
	my($cvsfile) = _pickWithMemoryFile("::nglayout.cvsloc");
	my($session) = MacCVS->new( $cvsfile );
	unless (defined($session)) { die "Checkout aborted. Cannot create session file: $session" }

	# activate MacCVS
	ActivateApplication('Mcvs');

	#//
	#//	Checkout commands
	#//
	if ($main::pull{all})
	{
		$session->checkout("SeaMonkeyEditor")						|| die "checkout failure";
  
		#// beard:  additional libraries needed to make shared libraries link.
		#//$session->checkout("mozilla/lib/mac/PowerPlant")	|| die "checkout failure";
		#//$session->checkout("mozilla/lib/xlate")				|| die "checkout failure";
	}
}


#//--------------------------------------------------------------------------------------------------
#// Build the 'dist' directory
#//--------------------------------------------------------------------------------------------------

sub BuildDist()
{
	unless ( $main::build{dist} ) { return;}
	_assertRightDirectory();
	
	# activate MacPerl
	ActivateApplication('McPL');

	# we really do not need all these paths, but many client projects include them
	mkpath([ ":mozilla:dist:", ":mozilla:dist:client:", ":mozilla:dist:client_debug:", ":mozilla:dist:client_stubs:" ]);
	mkpath([ ":mozilla:dist:viewer:", ":mozilla:dist:viewer_debug:" ]);

	my($distdirectory) = ":mozilla:dist";
	
	if ($main::MOZ_FULLCIRCLE)
	{
		if ( $main::DEBUG )
		{
			mkpath([ "$distdirectory:viewer_debug:TalkBack"]);
		} 
		else
		{
			mkpath([ "$distdirectory:viewer:TalkBack"]);
		}
	}
	
	#MAC_COMMON
	InstallFromManifest(":mozilla:build:mac:MANIFEST",								"$distdirectory:mac:common:");
	InstallFromManifest(":mozilla:lib:mac:NSStdLib:include:MANIFEST",				"$distdirectory:mac:common:");
	InstallFromManifest(":mozilla:lib:mac:MacMemoryAllocator:include:MANIFEST",		"$distdirectory:mac:common:");
	InstallFromManifest(":mozilla:lib:mac:Misc:MANIFEST",							"$distdirectory:mac:common:");
	InstallFromManifest(":mozilla:lib:mac:MoreFiles:MANIFEST",						"$distdirectory:mac:common:morefiles:");

	#INCLUDE
	InstallFromManifest(":mozilla:config:mac:MANIFEST",								"$distdirectory:config:");
	InstallFromManifest(":mozilla:config:mac:MANIFEST_config",						"$distdirectory:config:");
	InstallFromManifest(":mozilla:include:MANIFEST",								"$distdirectory:include:");		
	InstallFromManifest(":mozilla:cmd:macfe:pch:MANIFEST",							"$distdirectory:include:");
	InstallFromManifest(":mozilla:cmd:macfe:utility:MANIFEST",						"$distdirectory:include:");

	#NSPR	
    InstallFromManifest(":mozilla:nsprpub:pr:include:MANIFEST",						"$distdirectory:nspr:");		
    InstallFromManifest(":mozilla:nsprpub:pr:src:md:mac:MANIFEST",					"$distdirectory:nspr:mac:");		
    InstallFromManifest(":mozilla:nsprpub:lib:ds:MANIFEST",							"$distdirectory:nspr:");		
    InstallFromManifest(":mozilla:nsprpub:lib:libc:include:MANIFEST",				"$distdirectory:nspr:");		
    InstallFromManifest(":mozilla:nsprpub:lib:msgc:include:MANIFEST",				"$distdirectory:nspr:");

	#INTL
	#UCONV
	InstallFromManifest(":mozilla:intl:uconv:public:MANIFEST",						"$distdirectory:uconv:");
	InstallFromManifest(":mozilla:intl:uconv:ucvlatin:MANIFEST",					"$distdirectory:uconv:");
	InstallFromManifest(":mozilla:intl:uconv:ucvja:MANIFEST",						"$distdirectory:uconv:");
	InstallFromManifest(":mozilla:intl:uconv:ucvja2:MANIFEST",						"$distdirectory:uconv:");
#	InstallFromManifest(":mozilla:intl:uconv:ucvtw:MANIFEST",						"$distdirectory:uconv:");
#	InstallFromManifest(":mozilla:intl:uconv:ucvtw2:MANIFEST",						"$distdirectory:uconv:");
	InstallFromManifest(":mozilla:intl:uconv:ucvcn:MANIFEST",						"$distdirectory:uconv:");
#	InstallFromManifest(":mozilla:intl:uconv:ucvko:MANIFEST",						"$distdirectory:uconv:");
#	InstallFromManifest(":mozilla:intl:uconv:ucvth:MANIFEST",						"$distdirectory:uconv:");
#	InstallFromManifest(":mozilla:intl:uconv:ucvvt:MANIFEST",						"$distdirectory:uconv:");


	#UNICHARUTIL
	InstallFromManifest(":mozilla:intl:unicharutil:public:MANIFEST",				"$distdirectory:unicharutil");

	#LOCALE
	InstallFromManifest(":mozilla:intl:locale:public:MANIFEST",						"$distdirectory:locale:");

	#LWBRK
	InstallFromManifest(":mozilla:intl:lwbrk:public:MANIFEST",						"$distdirectory:lwbrk:");

	#STRRES
	InstallFromManifest(":mozilla:intl:strres:public:MANIFEST",						"$distdirectory:strres:");

	#JPEG
    InstallFromManifest(":mozilla:jpeg:MANIFEST",									"$distdirectory:jpeg:");

	#LIBREG
    InstallFromManifest(":mozilla:modules:libreg:include:MANIFEST",					"$distdirectory:libreg:");

	#XPCOM
    InstallFromManifest(":mozilla:xpcom:public:MANIFEST",							"$distdirectory:xpcom:");
	InstallFromManifest(":mozilla:xpcom:src:MANIFEST",								"$distdirectory:xpcom:");
	InstallFromManifest(":mozilla:xpcom:libxpt:public:MANIFEST",					"$distdirectory:xpcom:");
	InstallFromManifest(":mozilla:xpcom:libxpt:xptinfo:public:MANIFEST",			"$distdirectory:xpcom:");
	InstallFromManifest(":mozilla:xpcom:libxpt:xptcall:public:MANIFEST",			"$distdirectory:xpcom:");
	
	#ZLIB
    InstallFromManifest(":mozilla:modules:zlib:src:MANIFEST",						"$distdirectory:zlib:");

    #LIBJAR
    InstallFromManifest(":mozilla:modules:libjar:MANIFEST",                         "$distdirectory:libjar:");

	#LIBUTIL
    InstallFromManifest(":mozilla:modules:libutil:public:MANIFEST",					"$distdirectory:libutil:");

	#SUN_JAVA
    InstallFromManifest(":mozilla:sun-java:stubs:include:MANIFEST",					"$distdirectory:sun-java:");
    InstallFromManifest(":mozilla:sun-java:stubs:macjri:MANIFEST",					"$distdirectory:sun-java:");

	#NAV_JAVA
    InstallFromManifest(":mozilla:nav-java:stubs:include:MANIFEST",					"$distdirectory:nav-java:");
    InstallFromManifest(":mozilla:nav-java:stubs:macjri:MANIFEST",					"$distdirectory:nav-java:");

	#JS
    InstallFromManifest(":mozilla:js:src:MANIFEST",									"$distdirectory:js:");

	#LIVECONNECT
	InstallFromManifest(":mozilla:js:src:liveconnect:MANIFEST",						"$distdirectory:liveconnect:");
	
	#XPCONNECT
	InstallFromManifest(":mozilla:js:src:xpconnect:public:MANIFEST",				"$distdirectory:xpconnect:");
	
	#CAPS
	InstallFromManifest(":mozilla:caps:public:MANIFEST",							"$distdirectory:caps:");
	InstallFromManifest(":mozilla:caps:include:MANIFEST",							"$distdirectory:caps:");

	#SECURITY_freenav
    InstallFromManifest(":mozilla:modules:security:freenav:MANIFEST",				"$distdirectory:security:");

	#LIBPREF
    InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST",					"$distdirectory:libpref:");

	#PROFILE
    InstallFromManifest(":mozilla:profile:public:MANIFEST",							"$distdirectory:profile:");

	#LIBIMAGE
    InstallFromManifest(":mozilla:modules:libimg:png:MANIFEST",						"$distdirectory:libimg:");
    InstallFromManifest(":mozilla:modules:libimg:src:MANIFEST",						"$distdirectory:libimg:");
    InstallFromManifest(":mozilla:modules:libimg:public:MANIFEST",					"$distdirectory:libimg:");

	#PLUGIN
    InstallFromManifest(":mozilla:modules:plugin:nglsrc:MANIFEST",					"$distdirectory:plugin:");
    InstallFromManifest(":mozilla:modules:plugin:public:MANIFEST",					"$distdirectory:plugin:");
    InstallFromManifest(":mozilla:modules:plugin:src:MANIFEST",						"$distdirectory:plugin:");
    InstallFromManifest(":mozilla:modules:oji:src:MANIFEST",						"$distdirectory:oji:");
    InstallFromManifest(":mozilla:modules:oji:public:MANIFEST",						"$distdirectory:oji:");

	#DBM
    InstallFromManifest(":mozilla:dbm:include:MANIFEST",							"$distdirectory:dbm:");
	
	#LAYERS (IS THIS STILL NEEDED)
	InstallFromManifest(":mozilla:lib:liblayer:include:MANIFEST",					"$distdirectory:layers:");

	#NETWORK
    InstallFromManifest(":mozilla:network:public:MANIFEST",							"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:cache:MANIFEST",							"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:client:MANIFEST",							"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:cnvts:MANIFEST",							"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:cstream:MANIFEST",						"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:main:MANIFEST",							"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:mimetype:MANIFEST",						"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:util:MANIFEST",							"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:about:MANIFEST",					"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:certld:MANIFEST",				"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:dataurl:MANIFEST",				"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:file:MANIFEST",					"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:ftp:MANIFEST",					"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:gopher:MANIFEST",				"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:http:MANIFEST",					"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:js:MANIFEST",					"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:mailbox:MANIFEST",				"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:marimba:MANIFEST",				"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:nntp:MANIFEST",					"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:pop3:MANIFEST",					"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:remote:MANIFEST",				"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:smtp:MANIFEST",					"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:protocol:sockstub:MANIFEST",				"$distdirectory:network:");
    InstallFromManifest(":mozilla:network:module:MANIFEST",							"$distdirectory:network:module");

	#BASE
    InstallFromManifest(":mozilla:base:src:MANIFEST",								"$distdirectory:base:");
    InstallFromManifest(":mozilla:base:public:MANIFEST",							"$distdirectory:base:");

	#WALLET
    InstallFromManifest(":mozilla:extensions:wallet:public:MANIFEST",				"$distdirectory:wallet:");

	#WEBSHELL
    InstallFromManifest(":mozilla:webshell:public:MANIFEST",						"$distdirectory:webshell:");
    InstallFromManifest(":mozilla:webshell:tests:viewer:public:MANIFEST",  			"$distdirectory:webshell:");

	#LAYOUT
    InstallFromManifest(":mozilla:layout:build:MANIFEST",							"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:public:MANIFEST",						"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:html:document:src:MANIFEST",				"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:style:public:MANIFEST",				"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:style:src:MANIFEST",					"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:base:src:MANIFEST",					"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:html:forms:public:MANIFEST",				"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:table:public:MANIFEST",				"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:src:MANIFEST",						"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:events:public:MANIFEST",					"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:events:src:MANIFEST",						"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:xml:document:public:MANIFEST",				"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:xml:content:public:MANIFEST",				"$distdirectory:layout:");

	#GFX
    InstallFromManifest(":mozilla:gfx:public:MANIFEST",								"$distdirectory:gfx:");

	#VIEW
    InstallFromManifest(":mozilla:view:public:MANIFEST",							"$distdirectory:view:");

	#DOM
   InstallFromManifest(":mozilla:dom:public:MANIFEST",								"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:base:MANIFEST",							"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:coreDom:MANIFEST",						"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:coreEvents:MANIFEST",					"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:events:MANIFEST",						"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:range:MANIFEST",						"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:html:MANIFEST",							"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:css:MANIFEST",							"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST",							"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:src:base:MANIFEST",							"$distdirectory:dom:");

	#HTMLPARSER
   InstallFromManifest(":mozilla:htmlparser:src:MANIFEST",							"$distdirectory:htmlparser:");

   #EXPAT
   InstallFromManifest(":mozilla:expat:xmlparse:MANIFEST",							"$distdirectory:expat:");
      
	#WIDGET
    InstallFromManifest(":mozilla:widget:public:MANIFEST",							"$distdirectory:widget:");
    InstallFromManifest(":mozilla:widget:src:mac:MANIFEST",							"$distdirectory:widget:");

    #RDF
    InstallFromManifest(":mozilla:rdf:base:public:MANIFEST",						"$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:util:public:MANIFEST",						"$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:content:public:MANIFEST",						"$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:datasource:public:MANIFEST",					"$distdirectory:rdf:");
    InstallFromManifest(":mozilla:rdf:build:MANIFEST",								"$distdirectory:rdf:");
    
    #BRPROF
    InstallFromManifest(":mozilla:rdf:brprof:public:MANIFEST",						"$distdirectory:brprof:");
    
    #CHROME
    InstallFromManifest(":mozilla:rdf:chrome:public:MANIFEST",                      "$distdirectory:chrome:");
    

	#EDITOR
   InstallFromManifest(":mozilla:editor:public:MANIFEST",							"$distdirectory:editor:");
   InstallFromManifest(":mozilla:editor:txmgr:public:MANIFEST",						"$distdirectory:editor:txmgr");
   InstallFromManifest(":mozilla:editor:txtsvc:public:MANIFEST",					"$distdirectory:editor:txtsvc");
  
    #SILENTDL
    InstallFromManifest(":mozilla:silentdl:MANIFEST",								"$distdirectory:silentdl:");

    #XPINSTALL
    InstallFromManifest(":mozilla:xpinstall:public:MANIFEST",                       "$distdirectory:xpinstall:");

   #FULL CIRCLE    
   if ($main::MOZ_FULLCIRCLE)
   {
		InstallFromManifest(":ns:fullsoft:public:MANIFEST",							"$distdirectory");
	
		if ($main::DEBUG)
		{
			#InstallFromManifest(":ns:fullsoft:public:MANIFEST",					"$distdirectory:viewer_debug:");
		} 
		else
		{
			#InstallFromManifest(":ns:fullsoft:public:MANIFEST",					"$distdirectory:viewer:");
			InstallFromManifest(":ns:fullsoft:public:MANIFEST",						"$distdirectory");
		}
	}

	# XPAPPS
   InstallFromManifest(":mozilla:xpfe:AppCores:public:MANIFEST",					"$distdirectory:xpfe:");
   InstallFromManifest(":mozilla:xpfe:appshell:public:MANIFEST",					"$distdirectory:xpfe:");

	# MAILNEWS
   if ($main::build{mailnews})
   {
	   InstallFromManifest(":mozilla:mailnews:public:MANIFEST",							"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:base:public:MANIFEST",					"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:base:build:MANIFEST",						"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:base:src:MANIFEST",						"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:base:util:MANIFEST",						"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:compose:public:MANIFEST",					"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:db:mdb:public:MANIFEST",					"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:db:msgdb:public:MANIFEST",				"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:db:msgdb:build:MANIFEST",					"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:local:public:MANIFEST",					"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:local:build:MANIFEST",					"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:mime:public:MANIFEST",					"$distdirectory:mailnews:");
	   InstallFromManifest(":mozilla:mailnews:news:public:MANIFEST",					"$distdirectory:mailnews:");
	}

	#// To get out defines in all the project, dummy alias NGLayoutConfigInclude.h into MacConfigInclude.h
	MakeAlias(":mozilla:config:mac:NGLayoutConfigInclude.h",	":mozilla:dist:config:MacConfigInclude.h");
}


#//--------------------------------------------------------------------------------------------------
#// Build stub projects
#//--------------------------------------------------------------------------------------------------

sub BuildStubs()
{

	unless( $main::build{stubs} ) { return; }
	_assertRightDirectory();

	#//
	#// Clean projects
	#//
	BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              	"Stubs");
	BuildProjectClean(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",				"Stubs");
}


#//--------------------------------------------------------------------------------------------------
#// Build one project, and make the alias. Parameters
#// are project path, target name, make shlb alias (boolean), make xSYM alias (boolean)
#// 
#// Note that this routine assumes that the target name and the shared libary name
#// are the same.
#//--------------------------------------------------------------------------------------------------

sub BuildOneProject($$$$$$)
{
	my ($project_path, $target_name, $toc_file, $alias_shlb, $alias_xSYM, $component) = @_;

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();
	my($component_dir) = $component ? "Components:" : "";

	my($project_dir) = $project_path;
	$project_dir =~ s/:[^:]+$/:/;			# chop off leaf name

	if ($main::CLOBBER_LIBS)
	{
		unlink "$project_dir$target_name";				# it's OK if these fail
		unlink "$project_dir$target_name.xSYM";
	}
	
	if ($toc_file ne "")
	{
		ReconcileProject("$project_path", "$project_dir$toc_file");
	}
	
	BuildProject($project_path, $target_name);
	
	$alias_shlb ? MakeAlias("$project_dir$target_name", "$dist_dir$component_dir") : 0;
	$alias_xSYM ? MakeAlias("$project_dir$target_name.xSYM", "$dist_dir") : 0;
}


#//--------------------------------------------------------------------------------------------------
#// Build common projects
#//--------------------------------------------------------------------------------------------------

sub BuildCommonProjects()
{
	unless( $main::build{common} ) { return; }
	_assertRightDirectory();

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();

	#//
	#// Stub libraries
	#//
	BuildProject(":mozilla:modules:security:freenav:macbuild:NoSecurity.mcp",			"Security.o");

	#//
	#// Shared libraries
	#//
	if ( $main::CARBON )
	{
		if ( $main::CARBONLITE ) {
			BuildProject(":mozilla:cmd:macfe:projects:interfaceLib:Interface.mcp",			"Carbon Interfaces (Lite)");
		}
		else {
			BuildProject(":mozilla:cmd:macfe:projects:interfaceLib:Interface.mcp",			"Carbon Interfaces");		
		}
	}
	else
	{
		BuildProject(":mozilla:cmd:macfe:projects:interfaceLib:Interface.mcp",			"MacOS Interfaces");
	}
	
	BuildOneProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",					"NSRuntime$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp",			"MoreFiles.o");

	BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",		"MemAllocator$D.o");

	BuildOneProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",					"NSStdLib$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",					"NSPR20$D.shlb", "NSPR20.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:jpeg:macbuild:JPEG.mcp",							"JPEG$D.shlb", "JPEG.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:libreg:macbuild:libreg.mcp",				"libreg$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",						"xpcom$D.shlb", "xpcom.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:xpcom:libxpt:macbuild:libxpt.mcp",				"libxpt$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:js:macbuild:JavaScript.mcp",						"JavaScript$D.shlb", "JavaScript.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:js:macbuild:LiveConnect.mcp",						"LiveConnect$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:js:macbuild:XPConnect.mcp",						"XPConnect$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:zlib:macbuild:zlib.mcp",					"zlib$D.shlb", "zlib.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
    BuildOneProject(":mozilla:modules:libjar:macbuild:libjar.mcp",              "libjar$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:caps:macbuild:Caps.mcp",						 	"Caps$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:modules:oji:macbuild:oji.mcp",					"oji$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:base:macbuild:base.mcp",							"base$D.shlb", "base.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:libpref:macbuild:libpref.mcp",			"libpref$D.shlb", "libpref.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:profile:macbuild:profile.mcp",					"profile$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 0);

	# International projects
	BuildOneProject(":mozilla:intl:uconv:macbuild:uconv.mcp",					"uconv$D.shlb", "uconv.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvlatin.mcp",				"ucvlatin$D.shlb", "uconvlatin.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja.mcp",					"ucvja$D.shlb", "ucvja.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvja2.mcp",					"ucvja2$D.shlb", "ucvja2.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
#// Have not enable yet... place holder
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw.mcp",					"ucvtw$D.shlb", "ucvtw.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvtw2.mcp",					"ucvtw2$D.shlb", "ucvtw2.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvcn.mcp",					"ucvcn$D.shlb", "ucvcn.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvko.mcp",					"ucvko$D.shlb", "ucvko.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvvt.mcp",					"ucvvt$D.shlb", "ucvvt.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
#	BuildOneProject(":mozilla:intl:uconv:macbuild:ucvth.mcp",					"ucvth$D.shlb", "ucvth.toc", 1, $main::ALIAS_SYM_FILES, 1);
				
	BuildOneProject(":mozilla:intl:unicharutil:macbuild:unicharutil.mcp",		"unicharutil$D.shlb", "unicharutil.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:intl:locale:macbuild:locale.mcp",					"nslocale$D.shlb", "nslocale.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
	BuildOneProject(":mozilla:intl:lwbrk:macbuild:lwbrk.mcp",					"lwbrk$D.shlb", "lwbrk.toc", 1, $main::ALIAS_SYM_FILES, 1);
	
    BuildOneProject(":mozilla:intl:strres:macbuild:strres.mcp",					"strres$D.shlb", "strres.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:modules:libutil:macbuild:libutil.mcp",			"libutil$D.shlb", "libutil.toc", 1, $main::ALIAS_SYM_FILES, 0);

	ReconcileProject(":mozilla:modules:libimg:macbuild:png.mcp", 				":mozilla:modules:libimg:macbuild:png.toc");
	BuildProject(":mozilla:modules:libimg:macbuild:png.mcp",					"png$D.o");

	BuildOneProject(":mozilla:modules:libimg:macbuild:libimg.mcp",				"libimg$D.shlb", "libimg.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:network:macbuild:network.mcp",					"NetworkModular$D.shlb", "network.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:extensions:wallet:macbuild:wallet.mcp",							"wallet$D.shlb", "wallet.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:rdf:brprof:build:brprof.mcp",						"brprof$D.shlb", "brprof.toc", 1, $main::ALIAS_SYM_FILES, 1);
    BuildOneProject(":mozilla:rdf:chrome:build:chrome.mcp",                     "chrome$D.shlb", "chrome.toc", 1, $main::ALIAS_SYM_FILES, 1);
    
#// XXX moved this TEMPORARILY to layout while we sort out a dependency
#	BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",							"rdf$D.shlb", "rdf.toc", 1, $main::ALIAS_SYM_FILES, 1);
}


#//--------------------------------------------------------------------------------------------------
#// Make resource aliases for one directory
#//--------------------------------------------------------------------------------------------------

sub BuildFolderResourceAliases($$)
{
	my($src_dir, $dest_dir) = @_;
	
	# get a list of all the resource files
	opendir(SRCDIR, $src_dir) || die("can't open $src_dir");
	my(@resource_files) = readdir(SRCDIR);
	closedir(SRCDIR);
	
	# make aliases for each one into the dest directory
	for ( @resource_files ) {
		next if $_ eq "CVS";
		
		my($file_name) = $src_dir . $_;	
		print("Placing alias to file $file_name in $dest_dir\n");
		MakeAlias($file_name, $dest_dir);
	}
}


#//--------------------------------------------------------------------------------------------------
#// Make resource aliases
#//--------------------------------------------------------------------------------------------------

sub MakeResourceAliases()
{
	unless( $main::build{resources} ) { return; }
	_assertRightDirectory();


	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();


	#//
	#// Most resources should all go into the chrome dir eventually
	#//
	my($chrome_dir) = "$dist_dir" . "Chrome:";
	my($resource_dir) = "$dist_dir" . "res:";


	#//
	#// Make aliases of resource files
	#//
	MakeAlias(":mozilla:layout:html:document:src:ua.css",								"$resource_dir");
	MakeAlias(":mozilla:webshell:tests:viewer:resources:viewer.properties",				"$resource_dir");

	my($html_dir) = "$resource_dir" . "html:";
	MakeAlias(":mozilla:layout:html:base:src:broken-image.gif",							"$html_dir");

	my($throbber_dir) = "$resource_dir" . "throbber:";
#	BuildFolderResourceAliases(":mozilla:xpfe:xpviewer:src:resources:throbber:",		"$throbber_dir");
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:throbber:",				"$throbber_dir");
	
	my($samples_dir) = "$resource_dir" . "samples:";
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:samples:",				"$samples_dir");
	BuildFolderResourceAliases(":mozilla:webshell:tests:viewer:resources:",				"$samples_dir");
	
	my($toolbar_dir) = "$resource_dir" . "toolbar:";
#	BuildFolderResourceAliases(":mozilla:xpfe:xpviewer:src:resources:toolbar:",			"$toolbar_dir");

	my($rdf_dir) = "$resource_dir" . "rdf:";
	BuildFolderResourceAliases(":mozilla:rdf:resources:",								"$rdf_dir");
	
	# NOTE: this will change as we move the toolbar/appshell chrome files to a real place
	InstallResources(":mozilla:xpfe:browser:src:MANIFEST",								"$samples_dir");

	BuildFolderResourceAliases(":mozilla:xpfe:browser:samples:", 						"$samples_dir");
	MakeAlias(":mozilla:xpfe:browser:samples:sampleimages:", 							"$samples_dir");
	BuildFolderResourceAliases(":mozilla:xpfe:AppCores:xul:",							"$samples_dir");
	BuildFolderResourceAliases(":mozilla:xpfe:AppCores:xul:resources:",					"$toolbar_dir");
	MakeAlias(":mozilla:xpfe:AppCores:xul:resources:throbbingN.gif",					"$throbber_dir");
	
	if ($main::build{editor})
	{
		my($editor_chrome_dir) = "$chrome_dir" . "Editor";

		InstallResources(":mozilla:editor:ui:composer:content:MANIFEST",			"$editor_chrome_dir:composer:content:default:", 0);
		InstallResources(":mozilla:editor:ui:composer:skin:MANIFEST",				"$editor_chrome_dir:composer:skin:default:", 0);

		InstallResources(":mozilla:editor:ui:dialogs:content:MANIFEST",				"$editor_chrome_dir:dialogs:content:default:", 0);
		InstallResources(":mozilla:editor:ui:dialogs:skin:MANIFEST",				"$editor_chrome_dir:dialogs:skin:default:", 0);
	}

	if ($main::build{mailnews})
	{
		my($messenger_dir) = "$resource_dir" . "mailnews:messenger:";
		BuildFolderResourceAliases(":mozilla:mailnews:ui:messenger:resources:",				"$messenger_dir");	
		#		BuildFolderResourceAliases(":mozilla:mailnews:mime:emitters:xml:resources:",		"$messenger_dir");	
		
		my($compose_dir) = "$resource_dir" . "mailnews:compose:";
		BuildFolderResourceAliases(":mozilla:mailnews:ui:compose:resources:",				"$compose_dir");	
		
		my($msgpref_dir) = "$resource_dir" . "mailnews:preference:";
		BuildFolderResourceAliases(":mozilla:mailnews:ui:preference:resources:",			"$msgpref_dir");
	}	

	MakeAlias(":mozilla:rdf:chrome:build:registry.rdf",								"$chrome_dir");
}


#//--------------------------------------------------------------------------------------------------
#// Build NGLayout
#//--------------------------------------------------------------------------------------------------

sub BuildLayoutProjects()
{
	unless( $main::build{nglayout} ) { return; }
	_assertRightDirectory();

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();
	

	
	#//
	#// Make WasteLib alias
	#//
	local(*F);
	my($filepath, $appath, $psi) = (':mozilla:build:mac:idepath.txt');
	if (open(F, $filepath)) {
		$appath = <F>;
		close(F);
		#// beard: use pattern substitution to generate path to WasteLib. (thanks gordon!)
		$appath =~ s/[^:]*$/MacOS Support:WASTE 1.3 Distribution:WASTELib/;
		my($wastelibpath) = $appath;
		MakeAlias("$wastelibpath", "$dist_dir");
	}
	else {
		print STDERR "Can't find $filepath\n";
	}

	
	#//
	#// Build Layout projects
	#//

	BuildProject(":mozilla:expat:macbuild:expat.mcp",						"expat$D.o");

	BuildOneProject(":mozilla:htmlparser:macbuild:htmlparser.mcp",				"htmlparser$D.shlb", "htmlparser.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:dom:macbuild:dom.mcp",							"dom$D.shlb", "dom.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:gfx:macbuild:gfx.mcp",							"gfx$D.shlb", "gfx.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:modules:plugin:macbuild:plugin.mcp",				"plugin$D.shlb", "plugin.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:layout:macbuild:layout.mcp",						"layout$D.shlb", "layout.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:view:macbuild:view.mcp",							"view$D.shlb", "view.toc", 1, $main::ALIAS_SYM_FILES, 0);
	
	BuildOneProject(":mozilla:widget:macbuild:widget.mcp",						"widget$D.shlb", "widget.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:webshell:macbuild:webshell.mcp",					"webshell$D.shlb", "webshell.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:webshell:embed:mac:RaptorShell.mcp",				"RaptorShell$D.shlb", "RaptorShell.toc", 1, $main::ALIAS_SYM_FILES, 0);

	#// XXX this is here because of a very TEMPORARY dependency
	BuildOneProject(":mozilla:rdf:macbuild:rdf.mcp",							"rdf$D.shlb", "rdf.toc", 1, $main::ALIAS_SYM_FILES, 1);
    
    BuildOneProject(":mozilla:xpinstall:macbuild:xpinstall.mcp",                "xpinstall$D.shlb", "", 1, $main::ALIAS_SYM_FILES, 1);

}


#//--------------------------------------------------------------------------------------------------
#// Build Editor Projects
#//--------------------------------------------------------------------------------------------------

sub BuildEditorProjects()
{
	unless( $main::build{editor} ) { return; }
	_assertRightDirectory();
	
	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();

	BuildOneProject(":mozilla:editor:txmgr:macbuild:txmgr.mcp",					"EditorTxmgr$D.shlb", "txmgr.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:editor:guimgr:macbuild:EditorGuiManager.mcp",		"EditorGuiManager$D.shlb", "EditorGuiManager.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:editor:macbuild:editor.mcp",						"EditorCore$D.shlb", "EditorCore.toc", 1, $main::ALIAS_SYM_FILES, 1);

}


#//--------------------------------------------------------------------------------------------------
#// Build Viewer Projects
#//--------------------------------------------------------------------------------------------------

sub BuildViewerProjects()
{
	unless( $main::build{viewer} ) { return; }
	_assertRightDirectory();
	
	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();

	BuildOneProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",			"viewer$D", "viewer.toc", 0, 0, 0);

#	BuildOneProject(":mozilla:xpfe:macbuild:xpfeviewer.mcp",					"xpfeviewer$D.shlb", "xpfeviewer.toc", 0, 0, 0);
}


#//--------------------------------------------------------------------------------------------------
#// Build XPApp Projects
#//--------------------------------------------------------------------------------------------------

sub BuildXPAppProjects()
{
	unless( $main::build{xpapp} ) { return; }
	_assertRightDirectory();

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();

	BuildOneProject(":mozilla:xpfe:appshell:macbuild:AppShell.mcp",				"AppShell$D.shlb", "AppShell.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:xpfe:AppCores:macbuild:AppCores.mcp",				"AppCores$D.shlb", "AppCores.toc", 1, $main::ALIAS_SYM_FILES, 0);

	BuildOneProject(":mozilla:xpfe:bootstrap:macbuild:apprunner.mcp",			"apprunner$D", "apprunner.toc", 0, 0, 1);

}



#//--------------------------------------------------------------------------------------------------
#// Build MailNews Projects
#//--------------------------------------------------------------------------------------------------

sub BuildMailNewsProjects()
{
	unless( $main::build{mailnews} ) { return; }
	_assertRightDirectory();

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();

	BuildOneProject(":mozilla:mailnews:base:util:macbuild:msgUtil.mcp",					"MsgUtil$D.lib", "MsgUtil.toc", 0, 0, 0);

	BuildOneProject(":mozilla:mailnews:base:macbuild:msgCore.mcp",						"mailnews$D.shlb", "mailnews.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:compose:macbuild:msgCompose.mcp",				"MsgCompose$D.shlb", "MsgCompose.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:db:macbuild:msgDB.mcp",							"MsgDB$D.shlb", "MsgDB.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:local:macbuild:msglocal.mcp",					"MsgLocal$D.shlb", "MsgLocal.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:mime:macbuild:mime.mcp",							"Mime$D.shlb", "Mime.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:mime:emitters:html:macbuild:htmlEmitter.mcp",	"htmlEmitter$D.shlb", "htmlEmitter.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:mime:emitters:xml:macbuild:xmlEmitter.mcp",		"xmlEmitter$D.shlb", "xmlEmitter.toc", 1, $main::ALIAS_SYM_FILES, 1);

	BuildOneProject(":mozilla:mailnews:mime:emitters:raw:macbuild:rawEmitter.mcp",		"rawEmitter$D.shlb", "rawEmitter.toc", 1, $main::ALIAS_SYM_FILES, 1);

}



#//--------------------------------------------------------------------------------------------------
#// Build everything
#//--------------------------------------------------------------------------------------------------

sub BuildProjects()
{
	# activate CodeWarrior
	ActivateApplication('CWIE');

	BuildStubs();
	BuildCommonProjects();
	BuildLayoutProjects();
	BuildEditorProjects();
	MakeResourceAliases();
	BuildViewerProjects();
	BuildXPAppProjects();
	BuildMailNewsProjects();
}
