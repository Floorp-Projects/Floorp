# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is The Waterfall Java Plugin Module
#  
# The Initial Developer of the Original Code is Sun Microsystems Inc
# Portions created by Sun Microsystems Inc are Copyright (C) 2001
# All Rights Reserved.
# 
# $Id: rules.pl,v 1.1 2001/07/12 20:25:40 edburns%acm.org Exp $
# 
# Contributor(s):
# 
#     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
# 

$post_license = <<EOF

Contributor(s):

    Nikolay N. Igotti <nikolay.igotti\@Sun.Com>
EOF
;
# trick to prevent CVS from rewriting those comments
$post_license = "\$"."Id: CVS comments placeholder "."Exp \$\n".$post_license;

$license = "/work/checkin/MPL.txt";
$oldtree = "/work/checkin/wf";
$newtree = "/work/checkin/wf.new";

%mapping = (
	    ".*\\.cpp\$"             => "cpp",
	    ".*\\.c\$"               => "c",
	    ".*\\.h\$"               => "h",
	    ".*\\.java\$"            => "java",
	    ".*\\.bat\$"             => "bat",
	    ".*\\.BAT\$"             => "bat",
	    ".*\\.policy"            => "policy",
	    ".*\\.sh"                => "sh", 
	    ".*\\.csh"               => "csh",
	    "GNUmakefile"            => "GNUmakefile",
	    "Makefile"               => "Makefile",
	    "makefile\\.win"         => "makefile.win",
	    ".*\\.mk"                => "mk",
	    ".*\\.gmk"               => "gmk",
	    ".*\\.pl"                => "pl"
	    );

%prefixes = (
	     "cpp"    => "-*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- ",
	     "c"      => "-*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-",
	     "java"   => "-*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-",
             "h"      => "-*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-"
	 );
%comments_start = (
		   "cpp"         => "/* ",
		   "c"           => "/* ",
		   "h"           => "/* ",
		   "policy"      => "/* ",
		   "sh"          => "# ",
		   "csh"         => "# ",
		   "pl"          => "# ",
		   "GNUmakefile" => "# ",
		   "Makefile"    => "# ",
		   "makefile.win"=> "# ",
		   "java"        => "/* ",
		   "bat"         => "\@REM ",
		   "BAT"         => "\@REM ",
		   "mk"          => "# ",
		   "gmk"         => "# "    
	       );

%comments_middle = (
		    "cpp"         => " * ",
		    "c"           => " * ",
		    "h"           => " * ",
		    "policy"      => " * ",
		    "java"        => " * ",
		    "sh"          => "# ",
		    "csh"         => "# ",
		    "pl"         => "# ",
		    "GNUmakefile" => "# ",
		    "Makefile"    => "# ",
		    "makefile.win"=> "# ",
		    "mk"          => "# ",
		    "gmk"         => "# ",
		    "bat"         => "\@REM ",
		    "BAT"         => "\@REM "		    
		);

%comments_end = (
		 "cpp"         => " */",
		 "c"           => " */",
		 "h"           => " */",
		 "policy"      => " */",
		 "sh"          => "# ",
		 "csh"         => "# ",
		 "pl"          => "# ",
		 "GNUmakefile" => "# ",
		 "Makefile"    => "# ",
		 "makefile.win"=> "# ",
		 "java"        => " */ ",
		 "bat"         => "\@REM ",
		 "BAT"         => "\@REM ",
		 "mk"          => "# ",
		 "gmk"         => "# "
	     );

%ignore_dirs = (
		"CVS" => 1
		); 


# filtering procedure: 
#       input  - filename, type, list of file strings 
#       output - list of file strings
sub proceed
{
    my ($file, $type, @lines) = @_;
    my @newlines;

    my ($pre, $com1, $com2, $com3) = 
	    ($prefixes{$type}, $comments_start{$type}, 
	     $comments_middle{$type}, $comments_end{$type});
    # in shell scripts add only after #!<smth> execution comment
    if ($type eq "sh" || $type eq "csh" || $type eq "pl") {
	$_ = $lines[0];
	push @newlines, shift @lines if (/^\#\S*\!/);
    }

    push @newlines, "$com1$pre\n";
    push @newlines, "$com2\n" if ($pre);
    foreach (@lic) {
	push @newlines, "$com2$_";
    }
    
    my @post = split('\n', $post_license);
    foreach (@post) {
	push @newlines, "$com2$_\n";
    }
    push @newlines, "$com3\n\n";

    foreach (@lines) {	
	push @newlines, $_;
    }
    return @newlines;
}




