#!perl

use strict;

my $tab = "    ";
my $init_tab = $tab;
my $enum_decs = "";
my $class_decs = "";
my @name_array;
my $opcode_maxlen = 0;

#
# fields are:
#
# * super: Class to inherit from, if super is Instruction_(1|2|3), the script
#          will automatically append the correct template info based on |params|
# * super_has_print: Set to 1 if you want to inherit the print() and print_args()
#                    methods from the superclass, set to 0 (or just don't set)
#                    to generate print methods.
# * rem: Remark you want to show up after the enum def, and inside the class.
# * params: The parameter list expected by the constructor, you can specify a
#           default value, using the syntax, [ ("Type = default") ].
#
# class names will be generated based on the opcode mnemonic.  See the
# subroutine get_classname for the implementation.  Basically underscores will
# be removes and the class name will be WordCapped, using the positions where the
# underscores were as word boundries.  The only exception occurs when a word is
# two characters, in which case both characters will be capped,
# as in BRANCH_GT -> BranchGT.
#

#
# template definitions for compare, arithmetic, and conditional branch ops
#
my $compare_op =
  {
   super  => "Compare",
   super_has_print => 1,
   rem    => "dest, source",
   params => [ ("Register", "Register") ]
  };

my $math_op =
  {
   super  => "Arithmetic",
   super_has_print => 1,
   rem    => "dest, source1, source2",
   params => [ ("Register", "Register", "Register") ]
  };

my $cbranch_op =
  {
   super  => "GenericBranch",
   super_has_print => 1,
   rem    => "target label, condition",
   params => [ ("Label*", "Register") ]
  };


#
# op defititions
#
my %ops;
$ops{"NOP"} = 
  {
   super  => "Instruction",
   rem    => "do nothing and like it",
  };
$ops{"MOVE"} =
  {
   super  => "Instruction_2",
   rem    => "dest, source",
   params => [ ("Register", "Register") ]
  };
$ops{"LOAD_IMMEDIATE"} =
  {
   super  => "Instruction_2",
   rem    => "dest, immediate value (double)",
   params => [ ("Register", "double" ) ]
  };
$ops{"LOAD_NAME"} =
  {
   super  => "Instruction_2",
   rem    => "dest, name",
   params => [ ("Register", "StringAtom*" ) ]
  };
$ops{"SAVE_NAME"} =
  {
   super  => "Instruction_2",
   rem    => "name, source",
   params => [ ("StringAtom*", "Register") ]
  };
$ops{"NEW_OBJECT"} =
  {
   super  => "Instruction_1",
   rem    => "dest",
   params => [ ("Register") ]
  };
$ops{"NEW_ARRAY"} =
  {
   super  => "Instruction_1",
   rem    => "dest",
   params => [ ("Register") ]
  };
$ops{"GET_PROP"} =
  {
   super  => "Instruction_3",
   rem    => "dest, object, prop name",
   params => [ ("Register", "Register", "StringAtom*") ]
  };
$ops{"SET_PROP"} =
  {
   super  => "Instruction_3",
   rem    => "object, name, source",
   params => [ ("Register", "StringAtom*", "Register") ]
  };
$ops{"GET_ELEMENT"} =
  {
   super  => "Instruction_3",
   rem    => "dest, array, index",
   params => [ ("Register", "Register", "Register") ]
  };
$ops{"SET_ELEMENT"} =
  {
   super  => "Instruction_3",
   rem    => "base, source1, source2",
   params => [ ("Register", "Register", "Register") ]
  };
$ops{"ADD"}      = $math_op;
$ops{"SUBTRACT"} = $math_op;
$ops{"MULTIPLY"} = $math_op;
$ops{"DIVIDE"}   = $math_op;
$ops{"COMPARE_LT"} = $compare_op;
$ops{"COMPARE_LE"} = $compare_op;
$ops{"COMPARE_EQ"} = $compare_op;
$ops{"COMPARE_NE"} = $compare_op;
$ops{"COMPARE_GE"} = $compare_op;
$ops{"COMPARE_GT"} = $compare_op;
$ops{"NOT"} =
  {
   super  => "Instruction_2",
   rem    => "dest, source",
   params => [ ("Register", "Register") ]
  };
$ops{"BRANCH"} =
  {
   super  => "GenericBranch",
   rem    => "target label",
   params => [ ("Label*") ]
  };
$ops{"BRANCH_LT"} = $cbranch_op;
$ops{"BRANCH_LE"} = $cbranch_op;
$ops{"BRANCH_EQ"} = $cbranch_op;
$ops{"BRANCH_NE"} = $cbranch_op;
$ops{"BRANCH_GE"} = $cbranch_op;
$ops{"BRANCH_GT"} = $cbranch_op;
$ops{"RETURN"} =
  {
   super    => "Instruction_1",
   rem      => "return value",
   params   => [ ("Register") ]
  };
$ops{"RETURN_VOID"} =
  {
   super => "Instruction",
   rem   => "Return without a value"
  };
$ops{"CALL"} =
  {
   super  => "Instruction_3",
   rem    => "result, target, args",
   params => [ ("Register" , "Register", "RegisterList") ]
  };
$ops{"THROW"} =
  {
   super    => "Instruction_1",
   rem      => "exception value",
   params   => [ ("Register") ]
  };
$ops{"TRY"} =
  {
   super  => "Instruction_2",
   rem    => "catch target, finally target",
   params => [ ("Label*", "Label*") ]
  };
$ops{"ENDTRY"} = 
  {
   super  => "Instruction",
   rem    => "mmm, there is no try, only do",
  };
$ops{"JSR"} =
  {
   super  => "GenericBranch",
   rem    => "target",
   params => [ ("Label*") ]
  };
$ops{"RTS"} =
  {
   super => "Instruction",
   rem   => "Return to sender",
  };

#
# nasty perl code, you probably don't need to muck around below this line
#

my $k;

if (!$ARGV[0]) {
    # no args, collect all opcodes
    for $k (sort(keys(%ops))) {
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
    # $enum_decs, @name_aray, and $class_decs.
    my ($k) = @_;

    if (length($k) > $opcode_maxlen) {
        $opcode_maxlen = length($k);
    }

    my $c = $ops{$k};
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
        $printops_decl .= ($printops_body ne "" ?
                           "const JSValues& registers" :
                           "const JSValues& /*registers*/");
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

sub spew {
    # print the info in $enum_decs, @name_aray, and $class_decs to stdout.
    my $opname;

    print $tab . "enum ICodeOp {\n$enum_decs$tab};\n\n";
    print $tab . "static char *opcodeNames[] = {\n";

    for $opname (@name_array) {
        print "$tab$tab\"$opname";
        for (0 .. $opcode_maxlen - length($opname) - 1) {
            print " ";
        }
        print "\",\n"
    }
    print "$tab};\n\n";
    print $class_decs;
}

sub get_classname {
    # munge an OPCODE_MNEMONIC into a ClassName
    my ($enum_name) = @_;
    my @words = split ("_", $enum_name);
    my $cname = "";
    my $i;
    my $word;

    for $word (@words) {
        if (length($word) == 2) {
            $cname .= uc($word);
        } else {
            $cname .= uc(substr($word, 0, 1)) . lc(substr($word, 1));
        }
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

        if ($type eq "Register") {
            push (@oplist, "\"R\" << mOp$op");
        } elsif ($type eq "Label*") {
            push (@oplist, "\"Offset \" << mOp$op->mOffset");
        } elsif ($type eq "StringAtom*") {
            push (@oplist, "\"'\" << *mOp$op << \"'\"");
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

        if ($type eq "Register") {
            push (@oplist, "\"R\" << mOp$op << '=' << registers[mOp$op]");
        } elsif ($type eq "RegisterList") {
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
