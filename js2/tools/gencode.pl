use strict;

my $tab = "    ";
my $init_tab = $tab;
my $enum_decs = "";
my $class_decs = "";
my @name_array;
my $opcode_maxlen = 0;

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

my %ops;
$ops{"NOP"} = 
  {
   super  => "Instruction",
   rem    => "do nothing and like it",
   params => [ () ]
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
   params => [ ("Register" ) ]
  };
$ops{"NEW_ARRAY"} =
  {
   super  => "Instruction_1",
   rem    => "dest",
   params => [ ("Register" ) ]
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

my $k;

if (!$ARGV[0]) {
    for $k (sort(keys(%ops))) {
        &collect($k);
    }
} else {
    while ($k = pop(@ARGV)) {
        &collect ($k);
    }
}

&spew;

sub collect {
    my ($k) = @_;

    if (length($k) > $opcode_maxlen) {
        $opcode_maxlen = length($k);
    }

    my $c = $ops{$k};
    my $opname = $k;
    my $cname = get_classname ($k);
    my $super = $c->{"super"};
    my $constructor = $super;
    
    my $rem = $c->{"rem"};
    my ($dec_list, $call_list, $template_list) =
      &get_paramlists(@{$c->{"params"}});
    my $params = $call_list ? $opname . ", " . $call_list : $opname;

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
                    "$init_tab$tab$tab($params) " .
                    "{};\n");
    if (!$c->{"super_has_print"}) {
        my $printbody = &get_printbody(split (", ", $template_list));

        $class_decs .= ($init_tab . $tab . 
                         "virtual Formatter& print (Formatter& f) {\n" .
                         $init_tab . $tab . $tab . "f << opcodeNames[$opname]" .
                         $printbody . ";\n" .
                         $init_tab . $tab . $tab . "return f;\n" .
                         $init_tab . $tab . "}\n");
    } else {
        $class_decs .= $init_tab . $tab . 
          "/* print() inherited from $super */\n";
    }

    $class_decs .= $init_tab . "};\n\n";
}

sub spew {
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

sub get_printbody {
    my (@types) = @_;
    my $type;
    my @oplist;
    my $op = 1;
    my $in = $init_tab . $tab . $tab;

    for $type (@types) {
        print "type $type\n";

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
            
}
