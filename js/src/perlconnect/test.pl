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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
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

############################################################
# main part of the test script
############################################################

package main;
use JS;

BEGIN 
  { $| = 1; print "1..11\n"; }
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
print $jsval == 6 ? "ok $testc\n" : "not ok $testc\n";

############################################################
#second very simple test
$testc++;
$jsval = $cx->eval('"hallo";');
print $jsval eq "hallo" ? "ok $testc\n" : "not ok $testc\n";

############################################################
# third very simple test
$testc++;
$jsval = $cx->eval("1.23");
print $jsval == 1.23 ? "ok $testc\n" : "not ok $testc\n";

############################################################
#undef is little bit tricky
$testc++;
$jsval = $cx->eval('undefined');
print ! defined $jsval  ? "ok $testc\n" : "not ok $testc\n";

############################################################
#can ve tie js objects? (generally to hash, Arrays to arrays too)
$testc++;
$jsval = $cx->eval('foo = new Object(); foo.prop = 1; foo;');
my %hash;
#read js property
tie %hash, 'JS::Object', $jsval;
print $hash{prop} == 1  ? "ok $testc\n" : "not ok $testc\n";

############################################################
#set js propertry
$testc++;
$hash{prop2} = 2;
$jsval = $cx->eval('foo.prop2;');
print $jsval == 2  ? "ok $testc\n" : "not ok $testc\n";

############################################################
#tie array
$testc++;
my @arr;
$jsval = $cx->eval('arr = new Array(); arr[0] = 0; arr[1] = 1; arr;');
tie @arr, "JS::Object", $jsval;
print ((($#arr == 1) && ($arr[1] == 1)) ? "ok $testc\n" : "not ok $testc\n");

############################################################
# object delegation test
$testc++;
$cx->createObject(new Proxy("init_value"), "perlobj", 
		  { getObj   => \&Proxy::getObj,
		    getValue => \&Proxy::getValue,
		  });
$jsval = $cx->eval("perlobj.getValue()");
print $jsval eq "init_value"  ? "ok $testc\n" : "not ok $testc\n";

############################################################
# perl object returned to js
$testc++;
$jsval = $cx->eval("po = perlobj.getObj(); po.new_meth()");
print $jsval eq "Son::new_meth" ? "ok $testc\n" : "not ok $testc\n";


############################################################
# and what about inherited methods?
$testc++;
$jsval = $cx->eval("po.old_meth()");
print $jsval eq "Father::old_meth" ? "ok $testc\n" : "not ok $testc\n";

############################################################
# error test
$testc++;
my $line;
sub js_ErrorReporter {
    my ($msg, $file, $line, $linebuf, $token) = @_;
    die "line $line";
}
$cx->setErrorReporter( \&js_ErrorReporter );
eval { $cx->eval("x = 2 + 4;\nsecond line is wrong\n"); };
print $@ =~ /^line 1/ ? "ok $testc\n" : "not ok $testc\n";

############################################################
# cleanup
# so far we have to undef context value, to make sure
# it is disposed before runtome
undef $cx;
undef $rt;

__END__

############################################################
# the old disabled tests (should work)
############################################################

use JS;
# create new JS context
($js = new JS()) or warn "new JS() failed";
# eval for simple scalar cases
# int
($js->eval("100") == 100) or warn "Wrong value returned from eval()";
# and string
($js->eval("'test string'") eq 'test string') or warn "Wrong value returned from eval()";
# double TODO: double comparison?
($js->eval("1.25") == 1.25) or warn "Wrong value returned from eval()";
# more complex eval:
# return an object
$obj = $js->eval(q/
    x = new Object();
    x.t='a';
    x;
/) or warn "eval failed";
# tie this object to a hash
tie %hash, "JS::Object", $obj;
# try retrieving and manipulating values
$hash{foo} = 1200;
$_ = $hash{foo};
($_ == 1200) or warn "Wrong value returned from hash";
#
$hash{bar} = 'abcdef';
$_ = $hash{bar};
($_ eq 'abcdef') or warn "Wrong value returned from hash";
# exists/delete
(exists $hash{bar}) or warn "\$hash{bar} should exist";
delete $hash{bar};
(!exists $hash{bar}) or warn "\$hash{bar} should be deleted";
# exists/clear
(exists $hash{foo}) or warn "\$hash{foo} should exist";
undef %hash;
(!exists $hash{foo}) or warn "\$hash{foo} should be deleted";
