#!/usr/bin/perl
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is JavaScript 2 Prototype.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1997-1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the NPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the NPL or the GPL.
#
# Contributers:
#

use strict;
use jsicodes;

my $k;
my $count = 0;

for $k (sort(keys(%jsicodes::ops))) {
    $count++;
    print "    {\"$k\", {";
    my $c = $jsicodes::ops{$k};
    if ($c->{"params"}) {
        my @ot;
        my @params = @{$c->{"params"}};
        my $p;
        @ot = ();
        for $p (@params) {
            if ($p eq "ArgumentList") {
                push (@ot, "otArgumentList");
            } elsif ($p eq "BinaryOperator::BinaryOp") {
                push (@ot, "otBinaryOp");
            } elsif ($p eq "ICodeModule*") {
                push (@ot, "otICodeModule");
            } elsif ($p eq "JSClass*") {
                push (@ot, "otJSClass");
            } elsif ($p eq "JSFunction*") {
                push (@ot, "otJSFunction");
            } elsif ($p eq "JSString*") {
                push (@ot, "otJSString");
            } elsif ($p eq "JSType*") {
                push (@ot, "otJSType");
            } elsif ($p eq "Label*") {
                push (@ot, "otLabel");
            } elsif ($p eq "TypedRegister") {
                push (@ot, "otRegister");
            } elsif ($p eq "bool") {
                push (@ot, "otBool");
            } elsif ($p eq "const StringAtom*") {
                push (@ot, "otStringAtom");
            } elsif ($p eq "double") {
                push (@ot, "otDouble");
            } elsif ($p eq "uint32") {
                push (@ot, "otUInt32");
            } else {
                die "unknown parameter type '$p' for icode '$k'.\n";
            }
        }
        print (join (", ", @ot));
    } else {
        print "otNone";
    }
    print "}},\n";
}

print "\n\nstatic uint icodemap_size = $count\n";
