############################################################################
# PerlConnect support package. 8/3/98 2:50PM
# This packages implements private methods called from jsperl.c
# Please don't try to include in from Perl
# See README.html and JS.xs for information about this module.
############################################################################

package JS;
require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);
@EXPORT_OK = qw(perl_eval perl_resolve perl_call $js $ver);

# version string for the interpreter
$ver = "[Perl Interpreter: Version $] compiled under $^O]\n";
$DEBUG = undef;

############################################################################
# TODO: This will be added
############################################################################
sub AUTOLOAD        #7/28/98 8:24PM
{
    print "\nJS::AUTOLOAD: $AUTOLOAD, not implemented yet\n" if $DEBUG;
}   ##AUTOLOAD

############################################################################
# Evaluates the parameter and returns the return result of eval() as a
# reference
############################################################################
sub perl_eval       #7/15/98 5:13PM
{
    my($stmt) = shift;
    package main;
    my(@_js) = eval($stmt);
    package JS;
    my($_js) = (scalar(@_js)==1)?$_js[0]:\@_js;
    undef $js;
    $js = (ref $_js) ? $_js: \$_js;
    print "Failure in perl_call!" unless ref $js;
}   ##perl_eval

############################################################################
# Calls the procesure passed as the first parameter and passes the rest of
# the arguments to it. The return result is converted to a reference as
# before
############################################################################
sub perl_call       #7/21/98 2:16PM
{
    my($proc) = shift;
    my($_js);

    $proc =~ s/main:://g;
    #print "Calling $proc\n";
    package main;
    my(@_js) = &$proc(@_);
    package JS;
    #print "here: ", @_js, "\n";
    $_js = (scalar(@_js)==1)?$_js[0]:\@_js;
    undef $js;
    $js = (ref $_js) ? $_js: \$_js;
    #print ref $js;
    print "Failure in perl_call!" unless ref $js;
}   ##perl_call

############################################################################
# Takes the first parameter and tries to retrieve this variable
############################################################################
sub perl_resolve        #7/22/98 10:08AM
{
    my($name)  = shift;
    my(@parts) = split('::', $name);
    my($last_part)  = pop(@parts);

    # variable lookup -- variables must start with $, @, or %
    if($last_part =~ /^([\$\@\%])(.+)/){
        my($resolved_name) = "$1".join('::', @parts)."::$2";
        package main;
        my(@_js) = eval($resolved_name);
        package JS;
        my($_js) = (scalar(@_js)==1)?$_js[0]:\@_js;
        undef $js;
        $js = (ref $_js) ? $_js: \$_js;
    }else{
        $name =~ s/main:://g;
        # if this function exists
        # function -- set $js to 1 to indicate this
        if(eval "return defined(&main::$name)"){
            print "function $name\n" if $DEBUG;
            $js = 1;
        # module
        }else{
            print "must be a module\n" if $DEBUG;
            $js=2;
            return;
            # defined module -- try to do an eval and check $@ to trap errors
            # as a result, the module is automatically pre-use'd if it exists
            $name =~ s/main:://g;
            if(eval "use $name; return !(defined($@));"){
                $js = 2;
            # o.w. this module is undefined
            }else{
                $js = 3;
            }
        }
    }
}   ##perl_resolve

############################################################################
# Validates package name
############################################################################
sub perl_validate_package       #7/22/98 10:15AM
{
    print "perl_validate_package\n" if $DEBUG;
    my($name) = shift;
    print $name if $DEBUG;
    $js = $name?1:undef;
}   ##perl_validate_package

# test procedure
sub c{
    print "da!\n" if wantarray;
    print "Called!\n";
    return @_;
}

1;