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
my $binary_op =
  {
   super  => "Instruction_3",
   super_has_print => 1,
   rem    => "dest, source1, source2",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister") ]
  };

my $math_op =
  {
   super  => "Arithmetic",
   super_has_print => 1,
   rem    => "dest, source1, source2",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister") ]
  };

my $cbranch_op =
  {
   super  => "GenericBranch",
   super_has_print => 1,
   rem    => "target label, condition",
   params => [ ("Label*", "TypedRegister") ]
  };

my $unary_op =
  {
   super  => "Instruction_2",
   rem    => "dest, source",
   params => [ ("TypedRegister", "TypedRegister") ]
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
$ops{"DEBUGGER"} = 
  {
   super  => "Instruction",
   rem    => "drop to the debugger",
  };
$ops{"GENERIC_BINARY_OP"} =
  {
   super  => "Instruction_4",
   rem    => "dest, op, source1, source2",
   params => [ ("TypedRegister", "BinaryOperator::BinaryOp", "TypedRegister", "TypedRegister") ]
  };
$ops{"MOVE"} =
  {
   super  => "Instruction_2",
   rem    => "dest, source",
   params => [ ("TypedRegister", "TypedRegister") ]
  };
$ops{"LOAD_IMMEDIATE"} =
  {
   super  => "Instruction_2",
   rem    => "dest, immediate value (double)",
   params => [ ("TypedRegister", "double" ) ]
  };
$ops{"LOAD_BOOLEAN"} =
  {
   super  => "Instruction_2",
   rem    => "dest, immediate value (boolean)",
   params => [ ("TypedRegister", "bool" ) ]
  };
$ops{"LOAD_STRING"} =
  {
   super  => "Instruction_2",
   rem    => "dest, immediate value (string)",
   params => [ ("TypedRegister", "JSString*" ) ]
  };
$ops{"LOAD_NAME"} =
  {
   super  => "Instruction_2",
   rem    => "dest, name",
   params => [ ("TypedRegister", "const StringAtom*" ) ]
  };
$ops{"SUPER"} =
  {
   super  => "Instruction_1",
   rem    => "dest",
   params => [ ("TypedRegister" ) ]
  };
$ops{"SAVE_NAME"} =
  {
   super  => "Instruction_2",
   rem    => "name, source",
   params => [ ("const StringAtom*", "TypedRegister") ]
  };
$ops{"NEW_OBJECT"} =
  {
   super  => "Instruction_2",
   rem    => "dest, constructor",
   params => [ ("TypedRegister", "TypedRegister") ]
  };
$ops{"NEW_CLASS"} =
  {
   super  => "Instruction_2",
   rem    => "dest, class",
   params => [ ("TypedRegister", "JSClass*") ]
  };
$ops{"NEW_FUNCTION"} =
  {
   super  => "Instruction_2",
   rem    => "dest, ICodeModule", 
   params => [ ("TypedRegister", "ICodeModule*") ]
  };
$ops{"NEW_ARRAY"} =
  {
   super  => "Instruction_1",
   rem    => "dest",
   params => [ ("TypedRegister") ]
  };
$ops{"DELETE_PROP"} =
  {
   super  => "Instruction_3",
   rem    => "dest, object, prop name",
   params => [ ("TypedRegister", "TypedRegister", "const StringAtom*") ]
  };
$ops{"GET_PROP"} =
  {
   super  => "Instruction_3",
   rem    => "dest, object, prop name",
   params => [ ("TypedRegister", "TypedRegister", "const StringAtom*") ]
  };
$ops{"SET_PROP"} =
  {
   super  => "Instruction_3",
   rem    => "object, name, source",
   params => [ ("TypedRegister", "const StringAtom*", "TypedRegister") ]
  };
$ops{"GET_SLOT"} =
  {
   super  => "Instruction_3",
   rem    => "dest, object, slot number",
   params => [ ("TypedRegister", "TypedRegister", "uint32") ]
  };
$ops{"SET_SLOT"} =
  {
   super  => "Instruction_3",
   rem    => "object, slot number, source",
   params => [ ("TypedRegister", "uint32", "TypedRegister") ]
  };
$ops{"GET_STATIC"} =
  {
   super  => "Instruction_3",
   rem    => "dest, class, index",
   params => [ ("TypedRegister", "JSClass*", "uint32") ]
  };
$ops{"SET_STATIC"} =
  {
   super  => "Instruction_3",
   rem    => "class, index, source",
   params => [ ("JSClass*", "uint32", "TypedRegister") ]
  };
$ops{"STATIC_XCR"} =
  {
   super  => "Instruction_4",
   rem    => "dest, class, index, value",
   params => [ ("TypedRegister", "JSClass*", "uint32", "double") ]
  };
$ops{"SLOT_XCR"} =
  {
   super  => "Instruction_4",
   rem    => "dest, source, slot number, value",
   params => [ ("TypedRegister", "TypedRegister", "uint32", "double") ]
  };
$ops{"PROP_XCR"} =
  {
   super  => "Instruction_4",
   rem    => "dest, source, name, value",
   params => [ ("TypedRegister", "TypedRegister", "const StringAtom*", "double") ]
  };
$ops{"ELEM_XCR"} =
  {
   super  => "Instruction_4",
   rem    => "dest, base, index, value",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister", "double") ]
  };
$ops{"NAME_XCR"} =
  {
   super  => "Instruction_3",
   rem    => "dest, name, value",
   params => [ ("TypedRegister", "const StringAtom*", "double") ]
  };
$ops{"VAR_XCR"} =
  {
   super  => "Instruction_3",
   rem    => "dest, source, value",
   params => [ ("TypedRegister", "TypedRegister", "double") ]
  };
$ops{"GET_ELEMENT"} =
  {
   super  => "Instruction_3",
   rem    => "dest, base, index",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister") ]
  };
$ops{"SET_ELEMENT"} =
  {
   super  => "Instruction_3",
   rem    => "base, index, value",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister") ]
  };
$ops{"ADD"}        = $math_op;
$ops{"SUBTRACT"}   = $math_op;
$ops{"MULTIPLY"}   = $math_op;
$ops{"DIVIDE"}     = $math_op;
$ops{"REMAINDER"}  = $math_op;
$ops{"SHIFTLEFT"}  = $math_op;
$ops{"SHIFTRIGHT"} = $math_op;
$ops{"USHIFTRIGHT"}= $math_op;
$ops{"AND"}        = $math_op;
$ops{"OR"}         = $math_op;
$ops{"XOR"}        = $math_op;
$ops{"COMPARE_LT"} = $binary_op;
$ops{"COMPARE_LE"} = $binary_op;
$ops{"COMPARE_EQ"} = $binary_op;
$ops{"COMPARE_NE"} = $binary_op;
$ops{"COMPARE_GE"} = $binary_op;
$ops{"COMPARE_GT"} = $binary_op;
$ops{"COMPARE_IN"} = $binary_op;
$ops{"STRICT_EQ"}  = $binary_op;
$ops{"STRICT_NE"}  = $binary_op;
$ops{"INSTANCEOF"} = $binary_op;
$ops{"BITNOT"}     = $unary_op;
$ops{"NOT"}        = $unary_op;
$ops{"TEST"}       = $unary_op;
$ops{"NEGATE"}     = $unary_op;
$ops{"POSATE"}     = $unary_op;
$ops{"BRANCH"} =
  {
   super  => "GenericBranch",
   rem    => "target label",
   params => [ ("Label*") ]
  };
$ops{"BRANCH_TRUE"} = $cbranch_op;
$ops{"BRANCH_FALSE"} = $cbranch_op;
$ops{"RETURN"} =
  {
   super    => "Instruction_1",
   rem      => "return value",
   params   => [ ("TypedRegister") ]
  };
$ops{"RETURN_VOID"} =
  {
   super => "Instruction",
   rem   => "Return without a value"
  };
$ops{"CALL"} =
  {
   super  => "Instruction_4",
   rem    => "result, target, this, args",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister", "RegisterList") ]
  };
$ops{"DIRECT_CALL"} =
  {
   super  => "Instruction_3",
   rem    => "result, target, args",
   params => [ ("TypedRegister", "JSFunction *", "RegisterList") ]
  };
$ops{"GET_METHOD"} =
  {
   super  => "Instruction_3",
   rem    => "result, target base, index",
   params => [ ("TypedRegister", "TypedRegister", "uint32") ]
  };
$ops{"THROW"} =
  {
   super    => "Instruction_1",
   rem      => "exception value",
   params   => [ ("TypedRegister") ]
  };
$ops{"TRYIN"} =
  {
   super  => "Instruction_2",
   rem    => "catch target, finally target",
   params => [ ("Label*", "Label*") ]
  };
$ops{"TRYOUT"} = 
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
$ops{"WITHIN"} =
  {
   super  => "Instruction_1",
   rem    => "within this object",
   params => [ ("TypedRegister") ]
  };
$ops{"WITHOUT"} =
  {
   super  => "Instruction",
   rem    => "without this object",
  };
$ops{"CAST"} =
  {
   super  => "Instruction_3",
   rem    => "dest, rvalue, toType",
   params => [ ("TypedRegister", "TypedRegister", "JSType*") ]
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
    # $enum_decs, @name_array, and $class_decs.
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

            $printops_decl .= ($dec_list =~ /RegisterList/) ?
                               "const JSValues& registers" :
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
