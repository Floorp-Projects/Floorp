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
# The Original Code is JavaScript Core Tests.
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
##############################################################################
# HELLO, ARE YOU READING THIS?
# The opcode definition is now in jsicodes.pm, please go there to make changes.
##############################################################################

my $tab = "    ";
my $init_tab = $tab;
my $enum_decs = "";
my $class_decs = "";
my @name_array;
my $opcode_maxlen = 0;

my $k;

if (!$ARGV[0]) {
    # no args, collect all opcodes
    for $k (sort(keys(%jsicodes::ops))) {
        &collect($k);
    }
} else {
    # collect defs for only the opcodes specified on the command line
    while ($k = pop(@ARGV)) {
        &collect (uc($k));
    }
}

&spew;

sub collect {
    # grab the info from the $k record in $ops, and append it to
    # $enum_decs, @name_array, and $class_decs.
    my ($k) = @_;

    if (length($k) > $opcode_maxlen) {
        $opcode_maxlen = length($k);
    }

    my $c = $jsicodes::ops{$k};
    if (!$c) {
        die ("Unknown opcode, $k\n");
    }

    my $opname = $k;
    my $cname = get_classname ($k);
    my $super = $c->{"super"};
    my $constructor = $super;
	my @params;
	
	if ($c->{"params"}) {
		@params = @{$c->{"params"}};
	} else {
		@params = ();
	}
    
    my $rem = $c->{"rem"};
    my ($dec_list, $call_list, $template_list) =
      &get_paramlists(@params);
    my @types = split (", ", $template_list);

    my $constr_params = $call_list ? $opname . ", " . $call_list : $opname;

    if ($super =~ /Instruction_\d/) {
        $super .= "<" . $template_list . ">";
    }    
    
    push (@name_array, $opname);
    $enum_decs .= "$init_tab$tab$opname, /* $rem */\n";
    if ($super) {
        $class_decs .= ($init_tab . "class $cname : public $super {\n" .
                        $init_tab . "public:\n" .
                        $init_tab . $tab . "/* $rem */\n" .
                        $init_tab . $tab . "$cname ($dec_list) :\n" .
                        $init_tab . $tab . $tab . "$super\n" .
                        "$init_tab$tab$tab($constr_params) " .
                        "{};\n");

        if (!$c->{"super_has_print"}) {
            $class_decs .= ($init_tab . $tab . 
                             "virtual Formatter& print(Formatter& f) {\n" .
                             $init_tab . $tab . $tab . "f << opcodeNames[$opname]" .
                             &get_print_body(@types) . ";\n" .
                             $init_tab . $tab . $tab . "return f;\n" .
                             $init_tab . $tab . "}\n");

            my $printops_body = &get_printops_body(@types);
            my $printops_decl =  "virtual Formatter& printOperands(Formatter& f, ";

#            $printops_decl .= ($dec_list =~ /ArgumentList/) ?
#                               "const JSValues& registers" :
#                               "const JSValues& registers";
            $printops_decl .= ($printops_body eq "") ?
                               "const JSValues& /*registers*/" :
                               "const JSValues& registers";
            $printops_decl .= ") {\n";

            $class_decs .= ($init_tab . $tab . 
                            $printops_decl .
                            $printops_body .
                            $init_tab . $tab . $tab . "return f;\n" .
                            $init_tab . $tab . "}\n");

        } else {
            $class_decs .= $init_tab . $tab . 
              "/* print() and printOperands() inherited from $super */\n";
        }

        $class_decs .= $init_tab . "};\n\n";
    }
}

sub spew {
    # print the info in $enum_decs, @name_aray, and $class_decs to stdout.
    my $opname;
    
    print "// THIS FILE IS MACHINE GENERATED! DO NOT EDIT BY HAND!\n\n";
    
    print "#if !defined(OPCODE_NAMES)\n\n";

    print $tab . "enum {\n$enum_decs$tab};\n\n";
    
    print $class_decs;
    
    print "#else\n\n";

    print $tab . "char *opcodeNames[] = {\n";

    for $opname (@name_array) {
        print "$tab$tab\"$opname";
        for (0 .. $opcode_maxlen - length($opname) - 1) {
            print " ";
        }
        print "\",\n"
    }
    print "$tab};\n\n";

    print "#endif\n\n"
}

sub get_classname {
    # munge an OPCODE_MNEMONIC into a ClassName
    my ($enum_name) = @_;
    my @words = split ("_", $enum_name);
    my $cname = "";
    my $i = 0;
    my $word;

    for $word (@words) {
        if ((length($word) == 2) && ($i != 0)) {
            $cname .= uc($word);
        } else {
            $cname .= uc(substr($word, 0, 1)) . lc(substr($word, 1));
        }
        $i++;
    }
    
    return $cname;
}

sub get_paramlists {
    # parse the params entry (passed into @types) into various parameter lists
    # used in the class declaration
    my @types = @_;
    my @dec;
    my @call;
    my @tostr;
    my @template;
    my $op = 1;
    my $type;

    for $type (@types) {
        my $pfx;
        my $deref;
        my $member;
        my $default;

        ($type, $default) = split (" = ", $type);
        if ($default ne "") {
            $default = " = " . $default;
        }

        $pfx = $deref = "";
        $member = "mOp$op";

        push (@dec, "$type aOp$op" . "$default");
        push (@call, "aOp$op");
        push (@template, $type);
        $op++;
    }

    return (join (", ", @dec), join (", ", @call), join (", ", @template));
}

sub get_print_body {
    # generate the body of the print() function
    my (@types) = @_;
    my $type;
    my @oplist;
    my $op = 1;
    my $in = $init_tab . $tab . $tab;

    for $type (@types) {

        if ($type eq "TypedRegister") {
           push (@oplist, "mOp$op" );
        } elsif ($type eq "Label*") {
            push (@oplist, "\"Offset \" << ((mOp$op) ? mOp$op->mOffset : NotAnOffset)")
        } elsif ($type =~ /String/) {
            push (@oplist, "\"'\" << *mOp$op << \"'\"");
        } elsif ($type =~ /JSType *\*/) {
            push (@oplist, "\"'\" << mOp$op->getName() << \"'\"");
        } elsif ($type =~ /JSFunction *\*/) {
            push (@oplist, "\"JSFunction\"");
        } elsif ($type =~ /bool/) {
            push (@oplist, "\"'\" << ((mOp$op) ? \"true\" : \"false\") << \"'\"");
        } elsif ($type =~ /ICodeModule/) {
            push (@oplist, "\"ICodeModule\"");
        } elsif ($type =~ /FunctionDefinition/) {
            push (@oplist, "\"FunctionDefinition\"");
        } elsif ($type =~ /JSClass *\*/) {
            push (@oplist, "mOp$op->getName()");
        } else {
            push (@oplist, "mOp$op");
        }

        $op++;
    }

    my $rv = join (" << \", \" << ", @oplist);
    if ($rv ne "") {
        $rv = " << \"\\t\" << " . $rv;
    }
    
    return $rv;
}

sub get_printops_body {
    # generate the body of the printOperands() function
    my (@types) = @_;
    my $type;
    my @oplist;
    my $op = 1;
    my $in = $init_tab . $tab . $tab;

    for $type (@types) {

        if ($type eq "TypedRegister") {
            push (@oplist, "getRegisterValue(registers, mOp$op.first)");
#            push (@oplist, "mOp$op.first");
#            push (@oplist, "\"R\" << mOp$op.first << '=' << registers[mOp$op.first]");
        } elsif ($type eq "ArgumentList") {
            push (@oplist, "ArgList(mOp$op, registers)");
        }

        $op++;
    }

    my $rv = join (" << \", \" << ", @oplist);
    if ($rv ne "") {
        $rv = $init_tab . $tab . $tab . "f << " . $rv . ";\n";
    }
    
    return $rv;
}
