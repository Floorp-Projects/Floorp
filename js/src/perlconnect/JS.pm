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

############################################################################
# JS.pm is a Perl interface to JavaScript 3/2/99 5:23PM
# The packages implemented here include
#           JS, JS::Runtime, JS::Contents, JS::Object
# See README.html for more information about this module.
############################################################################

package JS;
$VERSION='0.03';
require Exporter;
use DynaLoader;
@ISA = qw(Exporter DynaLoader);
use strict;
use vars qw($VERSION);

sub boot        #7/31/98 5:28PM
{
    # this is to load the JS DLL at run-time
    bootstrap JS $VERSION;
    push @DynaLoader::dl_library_path, $ENV{'LD_LIBRARY_PATH'};
}   ##boot

sub new {
    my ($class, $maxbytes) = @_;
    my $self = new JS::Runtime($maxbytes);
    return $self;
}

############################################################
# JS::Runtime
############################################################
package JS::Runtime;
use strict;
use vars qw ($AUTOLOAD $DEBUG %CONTEXTS);

# we use %CONTEXT hash to remember all created contex. 
# reason of this is increase reference to context objects
# and ensure corret order of destructor calls

sub AUTOLOAD        #7/28/98 8:24PM
{
    print "\nJS::Runtime::AUTOLOAD: $AUTOLOAD, not implemented yet\n" if $DEBUG;
}   ##AUTOLOAD


# Constructor. Calls NewRuntime

sub new     #7/31/98 3:39PM
{
    warn "JS::Runtime::new\n" if $DEBUG;
    my($class, $maxbytes) = @_;
    my $self = JS::NewRuntime(scalar($maxbytes));
    return  $self;
}   ##new

sub createContext {
    my ($self, $stacksize) = @_;
    #my $cx = $self->NewContext($stacksize);
    my $cx = JS::Context->new($self, $stacksize);
#    $CONTEXTS{$self} = [] unless exists $CONTEXTS{$self};
#    push @{$CONTEXTS{$self}}, $cx;
    return $cx;
}

# Destructor for Runtimes
sub DESTROY     #7/31/98 4:54PM
{
    my $self = shift;
    warn "JS::Runtime::DESTROY\n" if $DEBUG;
    JS::DestroyRuntime($self);
}   ##DESTROY

############################################################
# JS::Context
############################################################
package JS::Context;
use vars qw($AUTOLOAD $DEBUG);

# sub AUTOLOAD        #7/28/98 8:24PM
# {
#     print "\nJS::Context::AUTOLOAD: $AUTOLOAD, not implemented yet\n" if $DEBUG;
# }   ##AUTOLOAD

sub new     #7/31/98 3:39PM
{
    warn "JS::Context::new\n" if $DEBUG;
    my($class, $rt, $stacksize) = @_;
    $class = ref $class || $class;
    my $cx = JS::Runtime::NewContext($rt, $stacksize);
    my $self = {
		_handle => $cx,
		_name => ("name_" . int (rand 1000)),
	       };
      $self->{_handle};
    bless $self, $class;
    return  $self;
}   ##new

use Data::Dumper;

sub exec {
    my ($self, $script) = @_;
    $self->exec_($script);
}

sub DESTROY     #7/31/98 4:54PM
{
    my $self = shift;
    warn "JS::Context::DESTROY\n" if $DEBUG;
    JS::Runtime::DestroyContext($self);
}   ##DESTROY


############################################################
# JS::Script
############################################################
package JS::Script;

sub new {
    my ($class, $cx, $code, $name) = @_;
    $class = ref $class || $class;
    my $self = {};
    bless $self, $class;
    $self->{_script} = $self->compileScript($cx, $code, $name);
    $self->{_root} = $self->rootScript($cx, $name);
    $self->{_cx} = $cx; #!!! dangerous
    $self->{_name} = $name;
    return $self;
}

sub DESTROY {
    my $self = shift;
    print "---> script destroy\n";
    my $cx = $self->{_cx};
    $self->destroyScript($cx);
}

############################################################
# JS::Object
############################################################
package JS::Object;
use vars qw($AUTOLOAD $DEBUG);

sub AUTOLOAD        #7/28/98 8:24PM
{
    $_ = $AUTOLOAD;
    $_ =~ s/.*://;
    print "\nJS::Object::AUTOLOAD: $_, not implemented yet\n" if $DEBUG;
}   ##AUTOLOAD



############################################################
# executable part
############################################################
#$JS::Runtime::DEBUG = 1;
#$JS::Context::DEBUG = 1;
#$JS::Object::DEBUG = 1;

&JS::boot();

1;

############################################################
# the end
############################################################




__END__

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
    $this = new JS::Context($rt, 1_024);
    return $this
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
warn("JS::Runtime::DESTROY\n");
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
warn("JS::Context::new\n");
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
    print "JS::Context::DESTROY\n" if $DEBUG;
warn("JS::Context::DESTROY\n");
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
