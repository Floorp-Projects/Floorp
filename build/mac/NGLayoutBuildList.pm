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

@ISA				= qw(Exporter);
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

#
# Utility routines
#

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

#
# MAIN ROUTINES
#
sub Checkout()
{
	_assertRightDirectory();
	my($cvsfile) = _pickWithMemoryFile("::nglayout.cvsloc");
	my($session) = MacCVS->new( $cvsfile );
	unless (defined($session)) { die "Checkout aborted. Cannot create session file: $session" }
	
	if ($main::pull{lizard})
	{
		$session->checkout("mozilla/LICENSE") || die "checkout failure";
		$session->checkout("mozilla/LEGAL") || die "checkout failure";
		$session->checkout("mozilla/config") || die "checkout failure";
		$session->checkout("mozilla/lib/liblayer") || die "checkout failure";
		$session->checkout("mozilla/modules/zlib") || die "checkout failure";
		$session->checkout("mozilla/modules/libutil") || die "checkout failure";
		$session->checkout("mozilla/nsprpub") || die "checkout failure";
		$session->checkout("mozilla/sun-java") || die "checkout failure";
		$session->checkout("mozilla/nav-java") || die "checkout failure";
		$session->checkout("mozilla/js") || die "checkout failure";
		$session->checkout("mozilla/modules/security/freenav") || die "checkout failure";
		$session->checkout("mozilla/modules/libpref") || die "checkout failure";
		$session->checkout("mozilla/lib/libparse") || die "checkout failure";
		$session->checkout("mozilla/lib/layout") || die "checkout failure";
		$session->checkout("mozilla/lib/libstyle") || die "checkout failure";
		$session->checkout("mozilla/lib/libpwcac") || die "checkout failure";
	}
	if ($main::pull{xpcom})
	{
		$session->checkout("mozilla/modules/libreg ") || die "checkout failure";
		$session->checkout("mozilla/xpcom") || die "checkout failure";
	}
	if ($main::pull{imglib})
	{
		my($IMGLIB_BRANCH) = "MODULAR_IMGLIB_BRANCH";

		$session->checkout("mozilla/jpeg ", $IMGLIB_BRANCH) || die "checkout failure";
		$session->checkout("mozilla/modules/libutil", $IMGLIB_BRANCH) || die "checkout failure";
		$session->checkout("mozilla/modules/libimg", $IMGLIB_BRANCH) || die "checkout failure";
	}
	if ($main::pull{netlib})
	{
		$session->checkout("mozilla/lib/xp ") || die "checkout failure";
		$session->checkout("mozilla/network") || die "checkout failure";
		$session->checkout("mozilla/include") || die "checkout failure";
	}
	if ($main::pull{nglayout})
	{
		$session->checkout("mozilla/base ") || die "checkout failure";
		$session->checkout("mozilla/dom") || die "checkout failure";
		$session->checkout("mozilla/gfx") || die "checkout failure";
		$session->checkout("mozilla/htmlparser") || die "checkout failure";
		$session->checkout("mozilla/layout") || die "checkout failure";
		$session->checkout("mozilla/view") || die "checkout failure";
		$session->checkout("mozilla/webshell") || die "checkout failure";
		$session->checkout("mozilla/widget") || die "checkout failure";
	}
	if ($main::pull{mac})
	{
		$session->checkout("mozilla/build/mac ") || die "checkout failure";
		$session->checkout("mozilla/cmd/macfe") || die "checkout failure";
		$session->checkout("mozilla/lib/mac/MacMemoryAllocator") || die "checkout failure";
		$session->checkout("mozilla/lib/mac/NSStdLib") || die "checkout failure";
		$session->checkout("mozilla/lib/mac/MoreFiles") || die "checkout failure";
		$session->checkout("mozilla/lib/mac/NSRuntime") || die "checkout failure";
		$session->checkout("mozilla/lib/mac/Misc") || die "checkout failure";
	}
}

# builds the dist directory
sub BuildDist()
{
	unless ( $main::build{dist} ) { return;}
	_assertRightDirectory();
	
	# we really do not need all these paths, but many client projects include them
	mkpath([ ":mozilla:dist:", ":mozilla:dist:client:", ":mozilla:dist:client_debug:", ":mozilla:dist:client_stubs:" ]);
	mkpath([ ":mozilla:dist:viewer:", ":mozilla:dist:viewer_debug:" ]);

	my($distdirectory) = ":mozilla:dist";

	my($distlist) = [
#MAC_COMMON
	[":mozilla:build:mac:MANIFEST", "$distdirectory:mac:common:"],
	[":mozilla:lib:mac:NSStdLib:include:MANIFEST", "$distdirectory:mac:common:"],
	[":mozilla:lib:mac:MacMemoryAllocator:include:MANIFEST", "$distdirectory:mac:common:"],
	[":mozilla:lib:mac:Misc:MANIFEST", "$distdirectory:mac:common:"],
	[":mozilla:lib:mac:MoreFiles:MANIFEST", "$distdirectory:mac:common:morefiles:"],
#INCLUDE
		[":mozilla:config:mac:MANIFEST", "$distdirectory:config:"],
		[":mozilla:include:MANIFEST", "$distdirectory:include:"],		
		[":mozilla:cmd:macfe:pch:MANIFEST", "$distdirectory:include:"],
#NSPR	
    [":mozilla:nsprpub:pr:include:MANIFEST", "$distdirectory:nspr:"],		
    [":mozilla:nsprpub:pr:src:md:mac:MANIFEST", "$distdirectory:nspr:mac:"],		
    [":mozilla:nsprpub:lib:ds:MANIFEST", "$distdirectory:nspr:"],		
    [":mozilla:nsprpub:lib:libc:include:MANIFEST", "$distdirectory:nspr:"],		
    [":mozilla:nsprpub:lib:msgc:include:MANIFEST", "$distdirectory:nspr:"],
#JPEG
    [":mozilla:jpeg:MANIFEST", "$distdirectory:jpeg:"],
#LIBREG
    [":mozilla:modules:libreg:include:MANIFEST", "$distdirectory:libreg:"],
#XPCOM
    [":mozilla:xpcom:src:MANIFEST", "$distdirectory:xpcom:"],
#ZLIB
    [":mozilla:modules:zlib:src:MANIFEST", "$distdirectory:zlib:"],
#LIBUTIL
    [":mozilla:modules:libutil:public:MANIFEST", "$distdirectory:libutil:"],
#SUN_JAVA
    [":mozilla:sun-java:stubs:include:MANIFEST", "$distdirectory:sun-java:"],
    [":mozilla:sun-java:stubs:macjri:MANIFEST", "$distdirectory:sun-java:"],
#NAV_JAVA
    [":mozilla:nav-java:stubs:include:MANIFEST", "$distdirectory:nav-java:"],
    [":mozilla:nav-java:stubs:macjri:MANIFEST", "$distdirectory:nav-java:"],
#JS
    [":mozilla:js:src:MANIFEST", "$distdirectory:js:"],
#SECURITY_freenav
    [":mozilla:modules:security:freenav:MANIFEST", "$distdirectory:security:"],
#LIBPREF
    [":mozilla:modules:libpref:public:MANIFEST", "$distdirectory:libpref:"],
#LIBIMAGE
    [":mozilla:modules:libimg:png:MANIFEST", "$distdirectory:libimg:"],
    [":mozilla:modules:libimg:src:MANIFEST", "$distdirectory:libimg:"],
    [":mozilla:modules:libimg:public:MANIFEST", "$distdirectory:libimg:"],
#PARSE
		[":mozilla:lib:libparse:MANIFEST",			"$distdirectory:libparse:"],
#OLD LAYOUT
		[":mozilla:lib:layout:MANIFEST",	"$distdirectory:layout:"],
#STYLE
		[":mozilla:lib:libstyle:MANIFEST",	 "$distdirectory:libstyle:"],
#LAYERS
		[":mozilla:lib:liblayer:include:MANIFEST",	"$distdirectory:layers:"],
#NETWORK
    [":mozilla:network:cache:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:client:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:cnvts:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:cstream:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:main:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:mimetype:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:util:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:about:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:certld:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:dataurl:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:file:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:ftp:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:gopher:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:http:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:js:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:mailbox:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:marimba:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:nntp:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:pop3:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:remote:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:protocol:smtp:MANIFEST", "$distdirectory:network:"],
    [":mozilla:network:module:MANIFEST","$distdirectory:network:module"],
#BASE
    [":mozilla:base:src:MANIFEST", "$distdirectory:base:"],
    [":mozilla:base:public:MANIFEST", "$distdirectory:base:"],
#WEBSHELL
    [":mozilla:webshell:public:MANIFEST", "$distdirectory:webshell:"],
#LAYOUT
    [":mozilla:layout:base:public:MANIFEST", "$distdirectory:layout:"],
    [":mozilla:layout:html:style:public:MANIFEST", "$distdirectory:layout:"],
    [":mozilla:layout:html:base:src:MANIFEST", "$distdirectory:layout:"],
    [":mozilla:layout:html:document:public:MANIFEST", "$distdirectory:layout:"],
#WIDGET
    [":mozilla:widget:public:MANIFEST", "$distdirectory:widget:"],
#GFX
    [":mozilla:gfx:src:MANIFEST", "$distdirectory:gfx:"],
#VIEW
    [":mozilla:view:public:MANIFEST", "$distdirectory:view:"],
#DOM
   [":mozilla:dom:public:MANIFEST", "$distdirectory:dom:"],
   [":mozilla:dom:public:coreDom:MANIFEST", "$distdirectory:dom:"],
   [":mozilla:dom:public:coreEvents:MANIFEST", "$distdirectory:dom:"],
   [":mozilla:dom:public:events:MANIFEST", "$distdirectory:dom:"],
#HTMLPARSER
   [":mozilla:htmlparser:src:MANIFEST", "$distdirectory:htmlparser:"],

	];
	foreach $a (@$distlist)
	{
		InstallFromManifest( $a->[0], $a->[1]);
	}
	
# To get out defines in all the project, dummy alias NGLayoutConfigInclude.h into MacConfigInclude.h
	MakeAlias(":mozilla:config:mac:NGLayoutConfigInclude.h", ":mozilla:dist:config:MacConfigInclude.h");
}

# builds all projects
# different targets controlled by $main::build
sub BuildCommonProjects()
{
	unless( $main::build{common} ) { return; }
	_assertRightDirectory();

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();

# clean projects

	Moz::BuildProjectClean(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              	"Stubs");
	Moz::BuildProjectClean(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp",							"Stubs");
	Moz::BuildProjectClean(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",	"Stubs");

# shared

	Moz::BuildProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp");
	MakeAlias(":mozilla:lib:mac:NSRuntime:NSRuntime$D.shlb", "$dist_dir");
	
	Moz::BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.mcp", "MoreFiles$D.shlb");
	MakeAlias(":mozilla:lib:mac:MoreFiles:build:MoreFiles$D.shlb", "$dist_dir");

	BuildProject(":mozilla:nsprpub:macbuild:NSPR20PPC.mcp",	"NSPR20$D.shlb");
	MakeAlias(":mozilla:nsprpub:macbuild:NSPR20$D.shlb", "$dist_dir");

	BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",	"MemAllocator$D.shlb");
	MakeAlias(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator$D.shlb", "$dist_dir");
	
	BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",								"NSStdLib$D.shlb");
	MakeAlias(":mozilla:lib:mac:NSStdLib:NSStdLib$D.shlb", "$dist_dir");
	
	BuildProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp",								"xpcom$D.shlb");
	MakeAlias(":mozilla:xpcom:macbuild:xpcom$D.shlb", "$dist_dir");

	BuildProject(":mozilla:jpeg:macbuild:JPEG.mcp",											"JPEG$D.shlb");
	MakeAlias(":mozilla:jpeg:macbuild:JPEG$D.shlb", "$dist_dir");

	BuildProject(":mozilla:js:macbuild:JavaScriptPPC.mcp",		"JavaScriptNoJSJ$D.shlb");
	MakeAlias(":mozilla:js:macbuild:JavaScript$D.shlb", "$dist_dir");

	BuildProject(":mozilla:modules:zlib:macbuild:zlib.mcp",		"zlib$D.shlb");
	MakeAlias(":mozilla:modules:zlib:macbuild:zlib$D.shlb", "$dist_dir");
	
# static

  BuildProject(":mozilla:modules:security:freenav:macbuild:NoSecurity.mcp",	 "Security.o");

	BuildProject(":mozilla:modules:libimg:macbuild:png.mcp",						"png$D.o");

	BuildProject(":mozilla:modules:libimg:macbuild:libimg.mcp",					"libimg$D.o");

	BuildProject(":mozilla:network:macbuild:network.mcp",		"networkModular$D.o");

}

sub BuildLayoutProjects()
{
	unless( $main::build{nglayout} ) { return; }
	_assertRightDirectory();

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? "Debug" : "";
	my($dist_dir) = _getDistDirectory();

	BuildProject(":mozilla:base:macbuild:base.mcp",				"base$D.o");

	BuildProject(":mozilla:htmlparser:macbuild:htmlparser.mcp",				"htmlparser$D.o");
	
	BuildProject(":mozilla:dom:macbuild:dom.mcp",				"dom$D.o");

	BuildProject(":mozilla:gfx:macbuild:gfx.mcp",				"gfx$D.o");

	BuildProject(":mozilla:layout:macbuild:layout.mcp",	"layout$D.o");

	BuildProject(":mozilla:webshell:macbuild:webshell.mcp",	"webshell$D.o");

	BuildProject(":mozilla:webshell:tests:viewer:mac:viewer.mcp",	"viewer$D");
}

sub BuildProjects()
{
	BuildCommonProjects();
	BuildLayoutProjects();
}
