#!perl -w
package			NGLayoutBuildList;

require 5.004;
require Exporter;

use strict;
use vars qw( @ISA @EXPORT );

# perl includes
use Mac::StandardFile;
use Mac::Processes;
use Cwd;
use File::Path;

# homegrown
use Moz;
use MacCVS;
use MANIFESTO;

@ISA			= qw(Exporter);
@EXPORT			= qw( Checkout BuildDist BuildProjects BuildCommonProjects BuildLayoutProjects);

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
		print "Choose a CVS session file in file dialog box:\n";	# no way to display a prompt?
# make sure that MacPerl is a front process
  while (GetFrontProcess	() !=  GetCurrentProcess())
  {
	   SetFrontProcess( GetCurrentProcess() );
  }
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
	_assertRightDirectory();
	my($cvsfile) = _pickWithMemoryFile("::nglayout.cvsloc");
	my($session) = MacCVS->new( $cvsfile );
	unless (defined($session)) { die "Checkout aborted. Cannot create session file: $session" }

	#//
	#//	Checkout commands
	#//
	if ($main::pull{all})
	{
		$session->checkout("RaptorMac")						|| die "checkout failure";
		#//$session->checkout("mozilla/modules/libpref")		|| die "checkout failure";
		
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
	
	# we really do not need all these paths, but many client projects include them
	mkpath([ ":mozilla:dist:", ":mozilla:dist:client:", ":mozilla:dist:client_debug:", ":mozilla:dist:client_stubs:" ]);
	mkpath([ ":mozilla:dist:viewer:", ":mozilla:dist:viewer_debug:" ]);

	my($distdirectory) = ":mozilla:dist";

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

	#JPEG
    InstallFromManifest(":mozilla:jpeg:MANIFEST",									"$distdirectory:jpeg:");

	#LIBREG
    InstallFromManifest(":mozilla:modules:libreg:include:MANIFEST",					"$distdirectory:libreg:");

	#XPCOM
    InstallFromManifest(":mozilla:xpcom:public:MANIFEST",							"$distdirectory:xpcom:");

	#ZLIB
    InstallFromManifest(":mozilla:modules:zlib:src:MANIFEST",						"$distdirectory:zlib:");

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

	#SECURITY_freenav
    InstallFromManifest(":mozilla:modules:security:freenav:MANIFEST",				"$distdirectory:security:");

	#LIBPREF
    InstallFromManifest(":mozilla:modules:libpref:public:MANIFEST",					"$distdirectory:libpref:");

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

	#LAYERS (IS THIS STILL NEEDED)
	InstallFromManifest(":mozilla:lib:liblayer:include:MANIFEST",					"$distdirectory:layers:");

	#NETWORK
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
    InstallFromManifest(":mozilla:network:module:MANIFEST",							"$distdirectory:network:module");

	#BASE
    InstallFromManifest(":mozilla:base:src:MANIFEST",								"$distdirectory:base:");
    InstallFromManifest(":mozilla:base:public:MANIFEST",							"$distdirectory:base:");

	#WEBSHELL
    InstallFromManifest(":mozilla:webshell:public:MANIFEST",						"$distdirectory:webshell:");

	#LAYOUT
    InstallFromManifest(":mozilla:layout:build:MANIFEST",							"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:public:MANIFEST",						"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:style:public:MANIFEST",				"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:style:src:MANIFEST",					"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:html:base:src:MANIFEST",					"$distdirectory:layout:");
    InstallFromManifest(":mozilla:layout:base:src:MANIFEST",						"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:events:public:MANIFEST",					"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:events:src:MANIFEST",						"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:xml:document:public:MANIFEST",				"$distdirectory:layout:");
	InstallFromManifest(":mozilla:layout:xml:content:public:MANIFEST",				"$distdirectory:layout:");

	#WIDGET
    InstallFromManifest(":mozilla:widget:public:MANIFEST",							"$distdirectory:widget:");
    InstallFromManifest(":mozilla:widget:src:mac:MANIFEST",							"$distdirectory:widget:");

	#GFX
    InstallFromManifest(":mozilla:gfx:src:MANIFEST",								"$distdirectory:gfx:");
    InstallFromManifest(":mozilla:gfx:public:MANIFEST",								"$distdirectory:gfx:");

	#VIEW
    InstallFromManifest(":mozilla:view:public:MANIFEST",							"$distdirectory:view:");

	#DOM
   InstallFromManifest(":mozilla:dom:public:MANIFEST",								"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:coreDom:MANIFEST",						"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:coreEvents:MANIFEST",					"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:events:MANIFEST",						"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:html:MANIFEST",							"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:public:css:MANIFEST",							"$distdirectory:dom:");
   InstallFromManifest(":mozilla:dom:src:jsurl:MANIFEST",							"$distdirectory:dom:");

	#HTMLPARSER
   InstallFromManifest(":mozilla:htmlparser:src:MANIFEST",							"$distdirectory:htmlparser:");
   
    #RDF
     InstallFromManifest(":mozilla:rdf:include:MANIFEST",							"$distdirectory:rdf:");

	#EDITOR
   InstallFromManifest(":mozilla:editor:public:MANIFEST",							"$distdirectory:editor:");
   InstallFromManifest(":mozilla:editor:txmgr:public:MANIFEST",						"$distdirectory:editor:txmgr");

	#// To get out defines in all the project, dummy alias NGLayoutConfigInclude.h into MacConfigInclude.h
	MakeAlias(":mozilla:config:mac:NGLayoutConfigInclude.h",	":mozilla:dist:config:MacConfigInclude.h");
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
	#// Clean projects
	#//
	Moz::BuildProjectClean(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",	"Stubs");
	Moz::BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              	"Stubs");
	Moz::BuildProjectClean(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",				"Stubs");
	Moz::BuildProjectClean(":mozilla:cmd:macfe:projects:client:Client.mcp",    		"Stubs");

	#//
	#// Stub libraries
	#//
	BuildProject(":mozilla:modules:security:freenav:macbuild:NoSecurity.mcp",			"Security.o");

	#//
	#// Shared libraries
	#//
	if ( $main::CARBON )
	{
		BuildProject(":mozilla:cmd:macfe:projects:interfaceLib:Interface.mcp",			"Carbon Interfaces");
	}
	else
	{
		BuildProject(":mozilla:cmd:macfe:projects:interfaceLib:Interface.mcp",			"MacOS Interfaces");
	}
		
	Moz::BuildProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",						"NSRuntime$D.shlb");
	MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb",							"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb.xSYM",		"$dist_dir") : 0;
	
	Moz::BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp",				"MoreFiles$D.shlb");
	MakeAlias(":mozilla:lib:mac:MoreFiles:build:MoreFiles$D.shlb",						"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:lib:mac:MoreFiles:build:MoreFiles$D.shlb.xSYM",	"$dist_dir") : 0;

	ReconcileProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp", 						":mozilla:nsprpub:macbuild:NSPR20.toc");
	BuildProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",								"NSPR20$D.shlb");
	MakeAlias(":mozilla:nsprpub:macbuild:NSPR20$D.shlb",								"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:nsprpub:macbuild:NSPR20$D.shlb.xSYM",			"$dist_dir") : 0;

	BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",					"MemAllocator$D.shlb");
	MakeAlias(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator$D.shlb",					"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator$D.shlb.xSYM","$dist_dir") : 0;
	
	BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",								"NSStdLib$D.shlb");
	MakeAlias(":mozilla:lib:mac:NSStdLib:NSStdLib$D.shlb",								"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:lib:mac:NSStdLib:NSStdLib$D.shlb.xSYM",			"$dist_dir") : 0;

	ReconcileProject(":mozilla:jpeg:macbuild:JPEG.mcp", 								":mozilla:jpeg:macbuild:JPEG.toc");
	BuildProject(":mozilla:jpeg:macbuild:JPEG.mcp",										"JPEG$D.shlb");
	MakeAlias(":mozilla:jpeg:macbuild:JPEG$D.shlb",										"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:jpeg:macbuild:JPEG$D.shlb.xSYM",					"$dist_dir") : 0;

	ReconcileProject(":mozilla:js:macbuild:JavaScriptPPC.mcp", 							":mozilla:js:macbuild:JavaScript.toc");
	BuildProject(":mozilla:js:macbuild:JavaScriptPPC.mcp",								"JavaScriptNoJSJ$D.shlb");
	MakeAlias(":mozilla:js:macbuild:JavaScript$D.shlb",									"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:js:macbuild:JavaScript$D.shlb.xSYM",				"$dist_dir") : 0;

	ReconcileProject(":mozilla:modules:zlib:macbuild:zlib.mcp", 						":mozilla:modules:zlib:macbuild:zlib.toc");
	BuildProject(":mozilla:modules:zlib:macbuild:zlib.mcp",								"zlib$D.shlb");
	MakeAlias(":mozilla:modules:zlib:macbuild:zlib$D.shlb",								"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:modules:zlib:macbuild:zlib$D.shlb.xSYM",			"$dist_dir") : 0;

	ReconcileProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp", 							":mozilla:xpcom:macbuild:xpcom.toc");
	BuildProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",								"xpcom$D.shlb");
	MakeAlias(":mozilla:xpcom:macbuild:xpcom$D.shlb",									"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:xpcom:macbuild:xpcom$D.shlb.xSYM",				"$dist_dir") : 0;

	ReconcileProject(":mozilla:modules:libpref:macbuild:libpref.mcp", 					":mozilla:modules:libpref:macbuild:libpref.toc");
	BuildProject(":mozilla:modules:libpref:macbuild:libpref.mcp",						"libpref$D.shlb");
	MakeAlias(":mozilla:modules:libpref:macbuild:libpref$D.shlb",						"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:modules:libpref:macbuild:libpref$D.shlb.xSYM",	"$dist_dir") : 0;
}


sub BuildResourceAliases
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
	#// Make aliases of resource files
	#//
	my($resource_dir) = "$dist_dir" . "res:";
	MakeAlias(":mozilla:layout:html:document:src:ua.css",								"$resource_dir");

	my($html_dir) = "$resource_dir" . "html:";
	MakeAlias(":mozilla:layout:html:base:src:broken-image.gif",							"$html_dir");

	my($throbber_dir) = "$resource_dir" . "throbber:";
	BuildResourceAliases(":mozilla:xpfe:xpviewer:src:resources:throbber:",				"$throbber_dir");
	
	my($samples_dir) = "$resource_dir" . "samples:";
	BuildResourceAliases(":mozilla:webshell:tests:viewer:samples:",						"$samples_dir");

	my($chrome_dir) = "$resource_dir" . "chrome:";
	BuildResourceAliases(":mozilla:xpfe:xpviewer:src:resources:chrome:",				"$chrome_dir");
	
	my($toolbar_dir) = "$resource_dir" . "toolbar:";
	BuildResourceAliases(":mozilla:xpfe:xpviewer:src:resources:toolbar:",				"$toolbar_dir");

	
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

	#// PowerPlant now used by widget, etc.
	BuildProject(":mozilla:lib:mac:PowerPlant:PowerPlant.mcp",							"PowerPlant$D.shlb");
	MakeAlias(":mozilla:lib:mac:PowerPlant:PowerPlant$D.shlb",							"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:lib:mac:PowerPlant:PowerPlant$D.shlb.xSYM",		"$dist_dir") : 0;

	ReconcileProject(":mozilla:base:macbuild:base.mcp", 								":mozilla:base:macbuild:base.toc");
	BuildProject(":mozilla:base:macbuild:base.mcp",										"base$D.shlb");
	MakeAlias(":mozilla:base:macbuild:base$D.shlb",										"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:base:macbuild:base$D.shlb.xSYM",					"$dist_dir") : 0;
	
	ReconcileProject(":mozilla:modules:libutil:macbuild:libutil.mcp", 					":mozilla:modules:libutil:macbuild:libutil.toc");
	BuildProject(":mozilla:modules:libutil:macbuild:libutil.mcp",						"libutil$D.shlb");
	MakeAlias(":mozilla:modules:libutil:macbuild:libutil$D.shlb",						"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:modules:libutil:macbuild:libutil$D.shlb.xSYM",	"$dist_dir") : 0;

	ReconcileProject(":mozilla:modules:libimg:macbuild:png.mcp", 						":mozilla:modules:libimg:macbuild:png.toc");
	BuildProject(":mozilla:modules:libimg:macbuild:png.mcp",							"png$D.o");
	ReconcileProject(":mozilla:modules:libimg:macbuild:libimg.mcp", 					":mozilla:modules:libimg:macbuild:libimg.toc");
	BuildProject(":mozilla:modules:libimg:macbuild:libimg.mcp",							"libimg$D.shlb");
	MakeAlias(":mozilla:modules:libimg:macbuild:libimg$D.shlb",							"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:modules:libimg:macbuild:libimg$D.shlb.xSYM",		"$dist_dir") : 0;

	#// beard:  now depends on libimg.
	ReconcileProject(":mozilla:network:macbuild:network.mcp", 							":mozilla:network:macbuild:network.toc");
	BuildProject(":mozilla:network:macbuild:network.mcp",								"NetworkModular$D.shlb");
	MakeAlias(":mozilla:network:macbuild:NetworkModular$D.shlb",						"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:network:macbuild:NetworkModular$D.shlb",			"$dist_dir") : 0;

	#// waterson: depends on NetworkModular and base. IMO we should move these to "common" projects
	ReconcileProject(":mozilla:rdf:macbuild:rdf.mcp", 									":mozilla:rdf:macbuild:rdf.toc");
	BuildProject(":mozilla:rdf:macbuild:rdf.mcp",										"rdf$D.shlb");
	MakeAlias(":mozilla:rdf:macbuild:rdf$D.shlb",										"$dist_dir");
	
	ReconcileProject(":mozilla:htmlparser:macbuild:htmlparser.mcp", 					":mozilla:htmlparser:macbuild:htmlparser.toc");
	BuildProject(":mozilla:htmlparser:macbuild:htmlparser.mcp",							"htmlparser$D.shlb");
	MakeAlias(":mozilla:htmlparser:macbuild:htmlparser$D.shlb",							"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:htmlparser:macbuild:htmlparser$D.shlb.xSYM",		"$dist_dir") : 0;
	
	ReconcileProject(":mozilla:dom:macbuild:dom.mcp", 									":mozilla:dom:macbuild:dom.toc");
	BuildProject(":mozilla:dom:macbuild:dom.mcp",										"dom$D.shlb");
	MakeAlias(":mozilla:dom:macbuild:dom$D.shlb",										"$dist_dir") ;
	$main::DEBUG ? MakeAlias(":mozilla:dom:macbuild:dom$D.shlb.xSYM",					"$dist_dir") : 0;

	ReconcileProject(":mozilla:gfx:macbuild:gfx.mcp", 									":mozilla:gfx:macbuild:gfx.toc");
	BuildProject(":mozilla:gfx:macbuild:gfx.mcp",										"gfx$D.shlb");
	MakeAlias(":mozilla:gfx:macbuild:gfx$D.shlb",										"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:gfx:macbuild:gfx$D.shlb.xSYM",					"$dist_dir") : 0;
	
	ReconcileProject(":mozilla:layout:macbuild:layout.mcp", 							":mozilla:layout:macbuild:layout.toc");
	BuildProject(":mozilla:layout:macbuild:layout.mcp",									"layout$D.shlb");
	MakeAlias(":mozilla:layout:macbuild:layout$D.shlb",									"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:layout:macbuild:layout$D.shlb.xSYM",				"$dist_dir") : 0;
	
	ReconcileProject(":mozilla:view:macbuild:view.mcp",									":mozilla:view:macbuild:view.toc");
	BuildProject(":mozilla:view:macbuild:view.mcp",										"view$D.shlb");
	MakeAlias(":mozilla:view:macbuild:view$D.shlb",										"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:view:macbuild:view$D.shlb.xSYM",					"$dist_dir") : 0;

	ReconcileProject(":mozilla:widget:macbuild:widget.mcp",								":mozilla:widget:macbuild:widget.toc");
	BuildProject(":mozilla:widget:macbuild:widget.mcp",									"widget$D.shlb");
	MakeAlias(":mozilla:widget:macbuild:widget$D.shlb",									"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:widget:macbuild:widget$D.shlb.xSYM",				"$dist_dir") : 0;
	
	ReconcileProject(":mozilla:webshell:macbuild:webshell.mcp",							":mozilla:webshell:macbuild:webshell.toc");
	BuildProject(":mozilla:webshell:macbuild:webshell.mcp",								"webshell$D.shlb");
	MakeAlias(":mozilla:webshell:macbuild:webshell$D.shlb",								"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:webshell:macbuild:webshell$D.shlb.xSYM",			"$dist_dir") : 0;
	
	ReconcileProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",					":mozilla:webshell:tests:viewer:mac:viewer.toc");
	BuildProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",						"viewer$D");

	ReconcileProject(":mozilla:xpfe:macbuild:xpfeviewer.mcp",							":mozilla:xpfe:macbuild:xpfeviewer.toc");
	BuildProject(":mozilla:xpfe:macbuild:xpfeviewer.mcp",								"xpfeViewer$D");
	
	ReconcileProject(":mozilla:xpfe:appshell:macbuild:AppShell.mcp",					":mozilla:xpfe:appshell:macbuild:AppShell.toc");
	BuildProject(":mozilla:xpfe:appshell:macbuild:AppShell.mcp",						"AppShell$D.shlb");
	MakeAlias(":mozilla:xpfe:appshell:macbuild:AppShell$D.shlb",						"$dist_dir");
	$main::DEBUG ? MakeAlias(":mozilla:xpfe:appshell:macbuild:AppShell$D.shlb.xSYM",	"$dist_dir") : 0;
	
	ReconcileProject(":mozilla:xpfe:bootstrap:macbuild:XPAppViewer.mcp",				":mozilla:xpfe:bootstrap:macbuild:XPAppViewer.toc");
	BuildProject(":mozilla:xpfe:bootstrap:macbuild:XPAppViewer.mcp",					"XPAppViewer$D");
}


#//--------------------------------------------------------------------------------------------------
#// Build everything
#//--------------------------------------------------------------------------------------------------

sub BuildProjects()
{
	BuildCommonProjects();
	BuildLayoutProjects();
}
