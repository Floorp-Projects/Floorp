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
# The Original Code is JavaScript Core Tests.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1997-1999
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

package jsicodes;

use strict;
use vars qw(%ops @ISA);

require Exporter;
@ISA = qw(Exporter);

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


#
# fields are:
#
# * super: Class to inherit from, if super is Instruction_(1|2|3), the script
#          will automatically append the correct template info based on
#          |params|
# * super_has_print: Set to 1 if you want to inherit the print() and
#                    print_args() methods from the superclass, set to 0 
#                    (or just don't set) to generate print methods.
#
# * rem: Remark you want to show up after the enum def, and inside the class.
# * params: The parameter list expected by the constructor, you can specify a
#           default value, using the syntax, [ ("Type = default") ].
#
# class names will be generated based on the opcode mnemonic.  See the
# subroutine get_classname for the implementation.  Basically underscores will
# be removes and the class name will be WordCapped, using the positions where
# the underscores were as word boundries.  The only exception occurs when a
# word is two characters, in which case both characters will be capped,
# as in BRANCH_GT -> BranchGT.
#

#
# template definitions for compare, arithmetic, and conditional branch ops
#
my $binary_op =
  {
   super  => "Instruction_3",
#   super_has_print => 1,
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
   params => [ ("TypedRegister", "JSTypes::Operator", "TypedRegister", "TypedRegister") ]
  };
$ops{"GENERIC_UNARY_OP"} =
  {
   super  => "Instruction_3",
   rem    => "dest, op, source",
   params => [ ("TypedRegister", "JSTypes::Operator", "TypedRegister") ]
  };
$ops{"GENERIC_XCREMENT_OP"} =
  {
   super  => "Instruction_3",
   rem    => "dest, op, source",
   params => [ ("TypedRegister", "JSTypes::Operator", "TypedRegister") ]
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
$ops{"LOAD_NULL"} =
  {
   super  => "Instruction_1",
   rem    => "dest",
   params => [ ("TypedRegister") ]
  };
$ops{"LOAD_TRUE"} =
  {
   super  => "Instruction_1",
   rem    => "dest",
   params => [ ("TypedRegister" ) ]
  };
$ops{"LOAD_FALSE"} =
  {
   super  => "Instruction_1",
   rem    => "dest",
   params => [ ("TypedRegister" ) ]
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
$ops{"LOAD_TYPE"} =
  {
   super  => "Instruction_2",
   rem    => "dest, type",
   params => [ ("TypedRegister", "JSType*" ) ]
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
$ops{"NEW_GENERIC"} =
  {
   super  => "Instruction_3",
   rem    => "dest, target, args",
   params => [ ("TypedRegister", "TypedRegister", "ArgumentList*") ]
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
$ops{"GET_FIELD"} =
  {
   super  => "Instruction_3",
   rem    => "dest, object, field",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister") ]
  };
$ops{"SET_FIELD"} =
  {
   super  => "Instruction_3",
   rem    => "object, field, source",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister") ]
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
$ops{"NEW_CLOSURE"} =
  {
   super  => "Instruction_2",
   rem    => "dest, ICodeModule",
   params => [ ("TypedRegister", "ICodeModule*") ]
  };
$ops{"GET_CLOSURE"} =
  {
   super  => "Instruction_2",
   rem    => "dest, closure depth",
   params => [ ("TypedRegister", "uint32") ]
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
$ops{"IS"}         = $binary_op;
$ops{"BITNOT"}     = $unary_op;
$ops{"NOT"}        = $unary_op;
$ops{"TEST"}       = $unary_op;
$ops{"NEGATE_DOUBLE"}     = $unary_op;
$ops{"POSATE_DOUBLE"}     = $unary_op;
$ops{"BRANCH"} =
  {
   super  => "GenericBranch",
   rem    => "target label",
   params => [ ("Label*") ]
  };
$ops{"BRANCH_TRUE"} = $cbranch_op;
$ops{"BRANCH_FALSE"} = $cbranch_op;
$ops{"BRANCH_INITIALIZED"} = $cbranch_op;
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
$ops{"DIRECT_CALL"} =
  {
   super  => "Instruction_3",
   rem    => "result, target, args",
   params => [ ("TypedRegister", "TypedRegister", "ArgumentList*") ]
  };
$ops{"INVOKE_CALL"} =
  {
   super  => "Instruction_3",
   rem    => "result, target, args",
   params => [ ("TypedRegister", "TypedRegister", "ArgumentList*") ]
  };
$ops{"BIND_THIS"} =
  {
   super  => "Instruction_3",
   rem    => "result, this, target",
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister") ]
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
   params => [ ("TypedRegister", "TypedRegister", "TypedRegister") ]
  };
$ops{"CLASS"} =
  {
   super  => "Instruction_2",
   rem    => "dest, obj",
   params => [ ("TypedRegister", "TypedRegister") ]
  };

1;
