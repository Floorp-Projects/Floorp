#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# Test file for JS.pm
#

############################################################
# support packages for test script
############################################################

############################################################
# Fotrik
package Father;

sub old_meth {
    return "Father::old_meth";
}


############################################################
# Synacek
package Son;
use vars qw( @ISA );
@ISA = qw( Father );

sub new {
    my $class = shift;
    $class = ref $class || $class;
    my $self = {};
    bless $self, $class;
    return $self;
}

sub new_meth {
    return "Son::new_meth";
}

############################################################
# Proxy
package Proxy;

sub new {
    my $class = shift;
    $class = ref $class || $class;
    my $self = { property => shift };
    bless $self, $class;
    return $self;    
}

sub getObj {
    my $ret = new Son();
    return $ret;
}

sub getValue {
    my $self = shift;
    return $self->{ property };
}

sub getArray {
    my $self = shift;
    return [34, 35, 36, 37, 38];
}

sub getHash {
    my $self = shift;
    return { testkey1 => 'testvalue1',
	     testkey2 => 'testvalue2',
	     testkey3 => 'testvalue3', };
}

############################################################
# main part of the test script
############################################################

package main;
use JS;

BEGIN { 
    $| = 1; print "1..12\n"; 
}

END 
  { print "not ok 1\n" unless $loaded; }

$loaded = 1;
print "ok 1\n";

use strict; #no typos, please

my $rt = new JS(1_204 ** 2);
my $cx = $rt->createContext(8 * 1_024);

my $jsval;
my $testc = 1; #testcounter
############################################################
# the simplest test
$testc++;
$jsval = $cx->eval('6;');
print $jsval == 6 ? "ok $testc\n" : "not ok $testc\n"; #2

############################################################
#second very simple test
$testc++;
$jsval = $cx->eval('"hallo";');
print $jsval eq "hallo" ? "ok $testc\n" : "not ok $testc\n"; #3

############################################################
# third very simple test
$testc++;
$jsval = $cx->eval("1.23");
print $jsval == 1.23 ? "ok $testc\n" : "not ok $testc\n"; #4

############################################################
#undef is little bit tricky
$testc++;
$jsval = $cx->eval('undefined');
print ! defined $jsval  ? "ok $testc\n" : "not ok $testc\n"; #5

############################################################
#can ve tie js objects? (generally to hash, Arrays to arrays too)
$testc++;
$jsval = $cx->eval('foo = new Object(); foo.prop = 11; foo;');
my %hash;
#read js property
tie %hash, 'JS::Object', $jsval;
print $hash{prop} == 11  ? "ok $testc\n" : "not ok $testc\n"; #6

############################################################
#set js propertry 
$testc++;
$hash{prop2} = 2;
$jsval = $cx->eval('foo.prop2;');
print $jsval == 2  ? "ok $testc\n" : "not ok $testc\n"; #7

############################################################
#tie array
$testc++;
my @arr;
$jsval = $cx->eval('arr = new Array(); arr[0] = 0; arr[1] = 1; arr;');
tie @arr, "JS::Object", $jsval;
print ((($#arr == 1) && ($arr[1] == 1)) ? "ok $testc\n" : "not ok $testc\n");#8

############################################################
# object delegation test
$testc++;
$cx->createObject(new Proxy("init_value"), "perlobj", 
		  { getObj   => \&Proxy::getObj,
		    getValue => \&Proxy::getValue,
		    getArray => \&Proxy::getArray,
		    getHash  => \&Proxy::getHash,
		  });
$jsval = $cx->eval("perlobj.getValue()");
print $jsval eq "init_value"  ? "ok $testc\n" : "not ok $testc\n"; #9

############################################################
# perl object returned to js
$testc++;
$jsval = $cx->eval("po = perlobj.getObj(); po.new_meth()");
print $jsval eq "Son::new_meth" ? "ok $testc\n" : "not ok $testc\n"; #10

############################################################
# and what about inherited methods?
$testc++;
$jsval = $cx->eval("po.old_meth()");
print $jsval eq "Father::old_meth" ? "ok $testc\n" : "not ok $testc\n"; #11

############################################################
# pass an array, check the element
$testc++;
$jsval = $cx->eval("parr = perlobj.getArray(); parr[2];");
print $jsval == 36 ? "ok $testc\n" : "not ok $testc\n"; #12

############################################################
# check the array length
$testc++;
$jsval = $cx->eval("parr.length");
print $jsval == 5 ? "ok $testc\n" : "not ok $testc\n"; #13

############################################################
# pass a hash, check the element
$testc++;
$jsval = $cx->eval("phash = perlobj.getHash(); phash.testkey1;");
print $jsval eq 'testvalue1' ? "ok $testc\n" : "not ok $testc\n"; #14

############################################################
# error test
$testc++;
my $line;
my $err;
sub js_ErrorReporter {
    my ($msg, $file, $line, $linebuf, $token) = @_;
    $err =  "line $line $msg";
}
$cx->setErrorReporter( \&js_ErrorReporter );
$cx->eval("x = 2 + 4;\nx.method()\n");
print $err =~ /^line 1/ ? "ok $testc\n" : "not ok $testc\n"; #15

############################################################
# cleanup
# so far we have to undef context value, to make sure,
# it is disposed before runtime
undef $cx;
undef $rt;

__END__


