/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

package com.compilercompany.ecmascript;

/**
 * GlobalObject
 *
 * An instance of this class is used to initialize a global
 * object.
 */

public final class GlobalObject implements Init {

    /**
     * Init
     */

    public void init( Value ob ) throws Exception {

        // Add slots for pre-defined types. A note on values
        // and types: Each type contains a set of values. A
        // value is an instance of the value class. For example,
        // new BooleanValue(true) is the true value of the boolean
        // type. BooleanValue.type is the type of all boolean
        // values. Value classes implement the Value interface.

        Slot slot;

        // Types

        slot = ob.add(null,"none");
        slot.type  = TypeType.type;
        slot.value = NoneType.type;

        slot = ob.add(null,"void");
        slot.type  = TypeType.type;
        slot.value = UndefinedType.type;

        slot = ob.add(null,"Null");
        slot.type  = TypeType.type;
        slot.value = NullType.type;

        slot = ob.add(null,"Boolean");
        slot.type  = TypeType.type;
        slot.value = BooleanType.type;

        slot = ob.add(null,"Integer");
        slot.type  = TypeType.type;
        slot.value = IntegerType.type;

        slot = ob.add(null,"Number");
        slot.type  = TypeType.type;
        slot.value = NumberType.type;

        slot = ob.add(null,"Character");
        slot.type  = TypeType.type;
        slot.value = CharacterType.type;

        slot = ob.add(null,"String");
        slot.type  = TypeType.type;
        slot.value = StringType.type;

        slot = ob.add(null,"Function");
        slot.type  = TypeType.type;
        slot.value = FunctionType.type;

        slot = ob.add(null,"Array");
        slot.type  = TypeType.type;
        slot.value = ArrayType.type;

        slot = ob.add(null,"Type");
        slot.type  = TypeType.type;
        slot.value = TypeType.type;

        slot = ob.add(null,"Unit");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        // Attributes

        slot = ob.add(null,"local");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"regional");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"global");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"extend");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"unit");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"public");
        slot.type  = NamespaceType.type;
        slot.value = new ObjectValue(new NamespaceType("public"));

        slot = ob.add(null,"internal");
        slot.type  = NamespaceType.type;
        slot.value = new ObjectValue(new NamespaceType("internal"));

        slot = ob.add(null,"private");
        slot.type  = NamespaceType.type;
        slot.value = new ObjectValue(new NamespaceType("private"));

        slot = ob.add(null,"implicit");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"explicit");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"indexable");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"nonindexable");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"final");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"dynamic");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"fixed");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"static");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"constructor");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"abstract");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"virtual");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"final");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"override");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"mayOverride");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"prototype");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"weak");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();

        slot = ob.add(null,"unused");
        slot.type  = ObjectType.type;
        slot.value = new ObjectValue();
    }
}

/*
 * The end.
 */

