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

############################################################################
# JS.pm is a Perl interface to JavaScript 3/2/99 5:23PM
# The packages implemented here include
#           JS, JS::Runtime, JS::Contents, JS::Object
# See README.html for more information about this module.
############################################################################

package JS;
$VERSION = '0.03';
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);

#@ISA = qw(JS);

############################################################################
# Duplicates JS::Context::new is a way
############################################################################
sub new         #7/31/98 5:32PM
{
    print "JS::new" if $DEBUG;
    my $rt = new JS::Runtime(10_000);
    return $this = new JS::Context($rt, 1_024);
}   ##new

############################################################################
# Uses DynaLoader to load JS support DLL, performs the initialization
############################################################################
sub boot        #7/31/98 5:28PM
{
    # this is to load the JS DLL at run-time
    bootstrap JS $VERSION;
    push @dl_library_path, $ENV{'LD_LIBRARY_PATH'};
}   ##boot

############################################################################
package JS::Runtime;
@EXPORT = qw($this);

sub AUTOLOAD        #7/28/98 8:24PM
{
    print "\nJS::Runtime::AUTOLOAD: $AUTOLOAD, not implemented yet\n" if $DEBUG;
}   ##AUTOLOAD

############################################################################
# Constructor. Calls NewRuntime and saves the returned value in this
############################################################################
sub new     #7/31/98 3:39PM
{
    print "JS::Runtime::new\n" if $DEBUG;
    my($class, $maxbytes) = @_;
    $this = JS::NewRuntime(scalar($maxbytes));
    return  $this;
}   ##new

############################################################################
# Destructor for Runtimes
############################################################################
sub DESTROY     #7/31/98 4:54PM
{
    my $self = shift;
    print "JS::Runtime::DESTROY\n" if $DEBUG;
    JS::DestroyRuntime($self);
    undef $this;
}   ##DESTROY

############################################################################
package JS::Context;
@EXPORT = qw($this);

sub AUTOLOAD        #7/28/98 8:24PM
{
    print "\nJS::Context::AUTOLOAD: $AUTOLOAD, not implemented yet\n" if $DEBUG;
}   ##AUTOLOAD

############################################################################
# Constructor. Calls NewContext and saves the returned value in this
############################################################################
sub new     #7/31/98 3:39PM
{
    print "JS::Context::new\n" if $DEBUG;
    my($class, $rt, $stacksize) = @_;
    $this = JS::Runtime::NewContext($rt, $stacksize);
    return  $this;
}   ##new

############################################################################
# Destructor for Contexts
############################################################################
sub DESTROY     #7/31/98 4:54PM
{
    my $self = shift;
    print "JS::Contexts::DESTROY\n" if $DEBUG;
    JS::Runtime::DestroyContext($self);
    undef $this;
}   ##DESTROY

############################################################################

package JS::Object;

sub AUTOLOAD        #7/28/98 8:24PM
{
    $_ = $AUTOLOAD;
    $_ =~ s/.*://;
    print "\nJS::Object::AUTOLOAD: $_, not implemented yet\n" if $DEBUG;
}   ##AUTOLOAD

&JS::boot();

1;