#!perl

#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

	use Moz;


chdir("::::"); # assuming this script is in "...:mozilla:build:mac:", change dir to just above "mozilla"
Moz::Configure(":Mozilla.Configuration");
Moz::OpenErrorLog("::Mozilla.BuildLog");


	#
	# Build the appropriate target of each project
	#

Moz::BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              				"Stub Library");
Moz::BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",				"Stub Library");
Moz::BuildProject(":mozilla:cmd:macfe:projects:client:Navigator.mcp",    				"Stub Library");

Moz::BuildProject(":mozilla:lib:mac:NSRuntime:NSRuntime.mcp");
Moz::BuildProject(":mozilla:cmd:macfe:restext:NavStringLibPPC.mcp");
Moz::BuildProject(":mozilla:lib:mac:MoreFiles:build:MoreFilesPPC.prj");
Moz::BuildProject(":mozilla:nsprpub:macbuild:NSPR20PPCDebug.mcp");
Moz::BuildProject(":mozilla:dbm:macbuild:DBMPPCDebug.mcp");
Moz::BuildProject(":mozilla:lib:mac:MacMemoryAllocator:MemAllocator.mcp",				"PPC Shared Library (Debug)");
Moz::BuildProject(":mozilla:lib:mac:NSStdLib:NSStdLib.mcp",              				"PPC Shared Library");
Moz::BuildProject(":mozilla:modules:security:freenav:macbuild:NoSecurity.mcp",	"PPC Shared Library (Debug)");
Moz::BuildProject(":mozilla:xpcom:macbuild:xpcomPPCDebug.mcp");
Moz::BuildProject(":mozilla:lib:mac:PowerPlant:PowerPlant.mcp");
Moz::BuildProject(":mozilla:modules:zlib:macbuild:zlib.mcp",            			  "PPC Shared Library (Debug)");
Moz::BuildProject(":mozilla:jpeg:macbuild:JPEG.mcp",                    			  "PPC Shared Library (Debug)");
Moz::BuildProject(":mozilla:sun-java:stubs:macbuild:JavaStubs.mcp",      				"PPC Shared Library (Debug)");
Moz::BuildProject(":mozilla:js:jsj:macbuild:JSJ_PPCDebug.mcp");
Moz::BuildProject(":mozilla:js:macbuild:JavaScriptPPCDebug.mcp");
Moz::BuildProject(":mozilla:nav-java:stubs:macbuild:NavJavaStubs.mcp",   				"PPC Shared Library (Debug)");
Moz::BuildProject(":mozilla:modules:rdf:macbuild:RDF.mcp",               				"PPC Shared Library +D -LDAP");
Moz::BuildProject(":mozilla:modules:xml:macbuild:XML.mcp",               				"PPC Shared Library (Debug)");
Moz::BuildProject(":mozilla:modules:libfont:macbuild:FontBroker.mcp",    				"PPC Library (Debug)");
Moz::BuildProject(":mozilla:modules:schedulr:macbuild:Schedulr.mcp",     				"PPC Shared Library (Debug)");
Moz::BuildProject(":mozilla:network:macbuild:network.mcp",											"PPC Library (Debug Moz)");
Moz::BuildProject(":mozilla:cmd:macfe:Composer:build:Composer.mcp",   					"PPC Library (Debug)");
Moz::BuildProject(":mozilla:cmd:macfe:projects:client:Navigator.mcp",   				"Moz PPC App (Debug)");
