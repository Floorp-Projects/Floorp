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