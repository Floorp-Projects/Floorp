#!/bin/perl

print "$ARGV[0]\n";

open (F, "<$ARGV[0]") || die ("Unable to open file $ARGV[0]\n");

$iid = "";
$in_iid = 0;
$if_name = "";
$if_body = "";

@interfaces = ("Node",
	       "NamedNodeMap",
	       "Document",
	       "CharacterData",
	       "CDATASection",
	       "Attr",
	       "Text",
	       "Element",
	       "DOMImplementation",
	       "Comment",
	       "Entity",
	       "ProcessingInstruction",
	       "HTML",
	       "CSS",
	       "Counter",
	       "Rect",
	       "RGBColor",
	       "StyleSheet",
	       "Event",
	       "UIEvent",
	       "PluginArray",
	       "Range",
	       "MediaList",
	       "AbstractView",
	       "CRMFObject",
	       "Crypto",
	       "Window",
	       "Plugin",
	       "MimeType",
	       "BarProp",
	       "Navigator",
	       "Screen",
	       "History",
	       "Pkcs11",
	       "XUL"
	       );

while (<F>) {
    if (/IID:/ || $in_iid) {
	chop();
	$iid .= $_;

	if ($in_iid) {
	    $in_iid = 0;
	} else {
	    $in_iid = 1;
	}

	$if_body = "";
    } elsif (/^\s*interface\s/) {
	$if_name = $_;
    } elsif (/^\s*\}\s*;/) {
	writeInterface();
    } else {
	s/xpidl //g;
	s/(\s)int(\s)/$1long$2/g;
	s/ longlong / long long /g;
	s/noscript/[noscript]/g;

	if (/replaceable/) {
	    s/replaceable //;
	    local $tmp = $_;
	    chop($tmp);
	    $tmp =~ s/(^\s*)//;

	    $if_body .= "\n$1/* [replaceable] $tmp */\n";

	    s/(^\s*)/$1\[noscript\] /;
	}

	if (/ function /) {
	    s/function //;

	    $if_body .= "\n /* XXX The nsIDOMEventListener arg should be flagged as function */\n";
	}

	s/(\s)_content/$1content/;

	$if_body .= $_;
    }
}

sub writeInterface() {
    $if_name =~ s/^\s+//;
    $if_name =~ s/\s+/ /g;
    $if_name =~ s/\{//g;
    $if_name =~ s/\s+$//;

    local $file_name = $if_name;

    $file_name =~ s/\s*interface\s+/nsIDOM/;
    $file_name =~ s/\s.*//;

    print "$file_name\n";

    open(idl, ">$file_name.idl") || die "Error opening file $file_name.idl: $!";

    $iid =~ s#IID:|[/*, \t{}\\]|0x##g;

    local @foo = split(//, $iid);
    local $if_decl = "";

    # Insert the '-' characters into the uuid string
    splice @foo, 20, 0, "-";
    splice @foo, 16, 0, "-";
    splice @foo, 12, 0, "-";
    splice @foo, 8, 0, "-";

    $LIST_SEPARATOR = ""; # why doesn't this work?

    $iid = "@foo";
    $iid =~ s/ //g; # if $LIST_SEPARATOR would work we shouldn't need this!

    if ($if_name =~ /:/) {
	local $tmp = $if_name;

	$tmp =~ s/[^:]*: *//;

	$if_decl = "#include \"$tmp.idl\"\n";
    } else {
	$if_decl = "#include \"domstubs.idl\"\n";
    }

    $if_decl .= "\n[scriptable, uuid(\L$iid\E)]\n";

    $if_decl .= $if_name;

    if ($if_name !~ /:/) {
	$if_decl .= " : nsISupports";
    }

    $if_decl .= "\n{\n";

    $if_decl .= $if_body;

    $if_decl .= "};\n\n";

    foreach $name (@interfaces) {
	$if_decl =~ s/([^a-zA-Z0-9])(N?S?$name)([a-zA-Z0-9_]*)/$1nsIDOM$2$3/g;
    }

    $if_decl =~ s/wstring/DOMString/g;

    print idl '/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 */

';
    print idl $if_decl;

    close idl;

    $iid = "";
    $if_name = "";
}
