#!perl -w
package			NGLayoutBuildList;

require 5.004;
require Exporter;

use strict;
use vars qw(@ISA @EXPORT $IMGLIB_BRANCH $distlist);

use MacCVS;
use Mac::StandardFile;
use Cwd;
use Moz;

@ISA				= qw(Exporter);
@EXPORT			= qw( Checkout BuildDist );

# NGLayoutBuildList builds the nglayout project
# it is configured by setting the following variables in the caller:
# Usage:
# caller variables that affect behaviour:
# DEBUG		: 1 if we are building a debug version
# 3-part build process: checkout, dist, and build_projects

#
# declarations
#
$IMGLIB_BRANCH = "MODULAR_IMGLIB_BRANCH";


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
	# prompt user for the file name, and store it
		print "Choose a CVS session file in file dialog box:\n";	# no way to display a prompt?
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
	}
	if ($main::pull{xpcom})
	{
		$session->checkout("mozilla/modules/libreg ") || die "checkout failure";
		$session->checkout("mozilla/xpcom") || die "checkout failure";
	}
	if ($main::pull{imglib})
	{
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
	}
}

# builds the dist directory
sub BuildDist()
{
	unless ( $main::build{dist} ) { return;}
	_assertRightDirectory();
	
	my($distlist) = [
#INCLUDE
		[":mozilla:config:mac:MANIFEST", ":mozilla:dist:config:"],
		[":mozilla:include:MANIFEST", ":mozilla:dist:include:"],		
		[":mozilla:cmd:macfe:pch:MANIFEST", ":mozilla:dist:include:"],
#NSPR	
    [":mozilla:nsprpub:pr:include:MANIFEST", ":mozilla:dist:nspr:"],		
    [":mozilla:nsprpub:pr:src:md:mac:MANIFEST", ":mozilla:dist:nspr:mac:"],		
    [":mozilla:nsprpub:lib:ds:MANIFEST", ":mozilla:dist:nspr:"],		
    [":mozilla:nsprpub:lib:libc:include:MANIFEST", ":mozilla:dist:nspr:"],		
    [":mozilla:nsprpub:lib:msgc:include:MANIFEST", ":mozilla:dist:nspr:"],
#JPEG
    [":mozilla:jpeg:MANIFEST", ":mozilla:dist:jpeg:"],
#LIBREG
    [":mozilla:modules:libreg:include:MANIFEST", ":mozilla:dist:libreg:"],
#XPCOM
    [":mozilla:xpcom:src:MANIFEST", ":mozilla:dist:xpcom:"],
#ZLIB
    [":mozilla:modules:zlib:src:MANIFEST", ":mozilla:dist:zlib:"],
#LIBUTIL
    [":mozilla:modules:libutil:public:MANIFEST", ":mozilla:dist:libutil:"],
#SUN_JAVA
    [":mozilla:sun-java:stubs:include:MANIFEST", ":mozilla:dist:sun-java:"],
    [":mozilla:sun-java:stubs:macjri:MANIFEST", ":mozilla:dist:sun-java:"],
#NAV_JAVA
    [":mozilla:nav-java:stubs:include:MANIFEST", ":mozilla:dist:nav-java:"],
    [":mozilla:nav-java:stubs:macjri:MANIFEST", ":mozilla:dist:nav-java:"],
#JS
    [":mozilla:js:src:MANIFEST", ":mozilla:dist:js:"],
#SECURITY_freenav
    [":mozilla:modules:security:freenav:MANIFEST", ":mozilla:dist:security:"],
#LIBPREF
    [":mozilla:modules:libpref:public:MANIFEST", ":mozilla:dist:libpref:"],
#LIBIMAGE
    [":mozilla:modules:libimg:png:MANIFEST", ":mozilla:dist:libimg:"],
    [":mozilla:modules:libimg:src:MANIFEST", ":mozilla:dist:libimg:"],
    [":mozilla:modules:libimg:public:MANIFEST", ":mozilla:dist:libimg:"],
#NETWORK
    [":mozilla:network:cache:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:client:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:cnvts:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:cstream:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:main:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:mimetype:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:util:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:about:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:certld:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:dataurl:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:file:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:ftp:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:gopher:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:http:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:js:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:mailbox:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:marimba:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:nntp:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:pop3:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:remote:MANIFEST", ":mozilla:dist:network:"],
    [":mozilla:network:protocol:smtp:MANIFEST", ":mozilla:dist:network:"],
#BASE
#XP
#error still not exported
	];
	foreach $a (@$distlist)
	{
		print "InstallFromManifest $a->[0] $a->[1]\n";
		InstallFromManifest( $a->[0], $a->[1]);
	}
}

# builds all projects
# different targets controlled by $main::build
sub BuildProjects()
{
	unless( $main::build{projects} ) { return; }
	_assertRightDirectory();

	# $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
	my($D) = $main::DEBUG ? " (Debug)" : "";
	
	Moz::BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              				"Stub Library");
	Moz::BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",				"Stub Library");
	Moz::BuildProject(":mozilla:cmd:macfe:projects:client:Navigator.mcp",    				"Stub Library");

	Moz::BuildProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp");
	Moz::BuildProject(":mozilla:cmd:macfe:restext:NavStringLibPPC.mcp");
	Moz::BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.prj");

	if ( $D )
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

	if ( $D )
	{
		Moz::BuildProject(":mozilla:xpcom:macbuild:xpcomPPCDebug.mcp");
	}
	else
	{
		Moz::BuildProject(":mozilla:xpcom:macbuild:xpcomPPC.mcp");
	}
	Moz::BuildProject(":mozilla:jpeg:macbuild:JPEG.mcp",                    			  "PPC Shared Library$D");

}
