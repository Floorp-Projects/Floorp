/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Roger Lawrence
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.classfile;

import org.mozilla.javascript.ObjToIntMap;
import org.mozilla.javascript.ObjArray;
import org.mozilla.javascript.UintMap;

import java.io.*;
import java.util.*;

/**
 * ClassFileWriter
 *
 * A ClassFileWriter is used to write a Java class file. Methods are
 * provided to create fields and methods, and within methods to write
 * Java bytecodes.
 *
 * @author Roger Lawrence
 */
public class ClassFileWriter {

    /**
     * Construct a ClassFileWriter for a class.
     *
     * @param className the name of the class to write, including
     *        full package qualification.
     * @param superClassName the name of the superclass of the class
     *        to write, including full package qualification.
     * @param sourceFileName the name of the source file to use for
     *        producing debug information, or null if debug information
     *        is not desired
     */
    public ClassFileWriter(String className, String superClassName,
                           String sourceFileName)
    {
        generatedClassName = className;
        itsConstantPool = new ConstantPool(this);
        itsThisClassIndex = itsConstantPool.addClass(className);
        itsSuperClassIndex = itsConstantPool.addClass(superClassName);
        if (sourceFileName != null)
            itsSourceFileNameIndex = itsConstantPool.addUtf8(sourceFileName);
        itsFlags = ACC_PUBLIC;
    }

    public final String getClassName()
    {
        return generatedClassName;
    }

    /**
     * Add an interface implemented by this class.
     *
     * This method may be called multiple times for classes that
     * implement multiple interfaces.
     *
     * @param interfaceName a name of an interface implemented
     *        by the class being written, including full package
     *        qualification.
     */
    public void addInterface(String interfaceName) {
        short interfaceIndex = itsConstantPool.addClass(interfaceName);
        itsInterfaces.add(new Short(interfaceIndex));
    }

    public static final short
        ACC_PUBLIC = 0x0001,
        ACC_PRIVATE = 0x0002,
        ACC_PROTECTED = 0x0004,
        ACC_STATIC = 0x0008,
        ACC_FINAL = 0x0010,
        ACC_SYNCHRONIZED = 0x0020,
        ACC_VOLATILE = 0x0040,
        ACC_TRANSIENT = 0x0080,
        ACC_NATIVE = 0x0100,
        ACC_ABSTRACT = 0x0400;

    /**
     * Set the class's flags.
     *
     * Flags must be a set of the following flags, bitwise or'd
     * together:
     *      ACC_PUBLIC
     *      ACC_PRIVATE
     *      ACC_PROTECTED
     *      ACC_FINAL
     *      ACC_ABSTRACT
     * TODO: check that this is the appropriate set
     * @param flags the set of class flags to set
     */
    public void setFlags(short flags) {
        itsFlags = flags;
    }

    static String getSlashedForm(String name)
    {
        return name.replace('.', '/');
    }

    /**
     * Convert Java class name in dot notation into
     * "Lname-with-dots-replaced-by-slashes;" form suitable for use as
     * JVM type signatures.
     */
    public static String classNameToSignature(String name)
    {
        int nameLength = name.length();
        int colonPos = 1 + nameLength;
        char[] buf = new char[colonPos + 1];
        buf[0] = 'L';
        buf[colonPos] = ';';
        name.getChars(0, nameLength, buf, 1);
        for (int i = 1; i != colonPos; ++i) {
            if (buf[i] == '.') {
                buf[i] = '/';
            }
        }
        return new String(buf, 0, colonPos + 1);
    }

    /**
     * Add a field to the class.
     *
     * @param fieldName the name of the field
     * @param type the type of the field using ...
     * @param flags the attributes of the field, such as ACC_PUBLIC, etc.
     *        bitwise or'd together
     */
    public void addField(String fieldName, String type, short flags) {
        short fieldNameIndex = itsConstantPool.addUtf8(fieldName);
        short typeIndex = itsConstantPool.addUtf8(type);
        itsFields.add(new ClassFileField(fieldNameIndex, typeIndex, flags));
    }

    /**
     * Add a field to the class.
     *
     * @param fieldName the name of the field
     * @param type the type of the field using ...
     * @param flags the attributes of the field, such as ACC_PUBLIC, etc.
     *        bitwise or'd together
     * @param value an initial integral value
     */
    public void addField(String fieldName, String type, short flags,
                         int value)
    {
        short fieldNameIndex = itsConstantPool.addUtf8(fieldName);
        short typeIndex = itsConstantPool.addUtf8(type);
        ClassFileField field = new ClassFileField(fieldNameIndex, typeIndex,
                                                  flags);
        field.setAttributes(itsConstantPool.addUtf8("ConstantValue"),
                            (short)0,
                            (short)0,
                            itsConstantPool.addConstant(value));
        itsFields.add(field);
    }

    /**
     * Add a field to the class.
     *
     * @param fieldName the name of the field
     * @param type the type of the field using ...
     * @param flags the attributes of the field, such as ACC_PUBLIC, etc.
     *        bitwise or'd together
     * @param value an initial long value
     */
    public void addField(String fieldName, String type, short flags,
                         long value)
    {
        short fieldNameIndex = itsConstantPool.addUtf8(fieldName);
        short typeIndex = itsConstantPool.addUtf8(type);
        ClassFileField field = new ClassFileField(fieldNameIndex, typeIndex,
                                                  flags);
        field.setAttributes(itsConstantPool.addUtf8("ConstantValue"),
                            (short)0,
                            (short)2,
                            itsConstantPool.addConstant(value));
        itsFields.add(field);
    }

    /**
     * Add a field to the class.
     *
     * @param fieldName the name of the field
     * @param type the type of the field using ...
     * @param flags the attributes of the field, such as ACC_PUBLIC, etc.
     *        bitwise or'd together
     * @param value an initial double value
     */
    public void addField(String fieldName, String type, short flags,
                         double value)
    {
        short fieldNameIndex = itsConstantPool.addUtf8(fieldName);
        short typeIndex = itsConstantPool.addUtf8(type);
        ClassFileField field = new ClassFileField(fieldNameIndex, typeIndex,
                                                  flags);
        field.setAttributes(itsConstantPool.addUtf8("ConstantValue"),
                            (short)0,
                            (short)2,
                            itsConstantPool.addConstant(value));
        itsFields.add(field);
    }

    /**
     * Add Information about java variable to use when generating the local
     * variable table.
     *
     * @param name variable name.
     * @param type variable type as bytecode descriptor string.
     * @param startPC the starting bytecode PC where this variable is live,
     *                 or -1 if it does not have a Java register.
     * @param register the Java register number of variable
     *                 or -1 if it does not have a Java register.
     */
    public void addVariableDescriptor(String name, String type, int startPC, int register)
    {
        int nameIndex = itsConstantPool.addUtf8(name);
        int descriptorIndex = itsConstantPool.addUtf8(type);
        int [] chunk = { nameIndex, descriptorIndex, startPC, register };
        if (itsVarDescriptors == null) {
            itsVarDescriptors = new ObjArray();
        }
        itsVarDescriptors.add(chunk);
    }

    /**
     * Add a method and begin adding code.
     *
     * This method must be called before other methods for adding code,
     * exception tables, etc. can be invoked.
     *
     * @param methodName the name of the method
     * @param type a string representing the type
     * @param flags the attributes of the field, such as ACC_PUBLIC, etc.
     *        bitwise or'd together
     */
    public void startMethod(String methodName, String type, short flags) {
        short methodNameIndex = itsConstantPool.addUtf8(methodName);
        short typeIndex = itsConstantPool.addUtf8(type);
        itsCurrentMethod = new ClassFileMethod(methodNameIndex, typeIndex,
                                               flags);
        itsMethods.add(itsCurrentMethod);
    }

    /**
     * Complete generation of the method.
     *
     * After this method is called, no more code can be added to the
     * method begun with <code>startMethod</code>.
     *
     * @param maxLocals the maximum number of local variable slots
     *        (a.k.a. Java registers) used by the method
     * @param vars the array of the variables for the method,
     *        or null if none
     */
    public void stopMethod(short maxLocals) {
        if (itsCurrentMethod == null)
            throw new IllegalStateException("No method to stop");

        fixLabelGotos();

        itsMaxLocals = maxLocals;

        int lineNumberTableLength = 0;
        if (itsLineNumberTable != null) {
            // 6 bytes for the attribute header
            // 2 bytes for the line number count
            // 4 bytes for each entry
            lineNumberTableLength = 6 + 2 + (itsLineNumberTableTop * 4);
        }

        int variableTableLength = 0;
        if (itsVarDescriptors != null) {
            // 6 bytes for the attribute header
            // 2 bytes for the variable count
            // 10 bytes for each entry
            variableTableLength = 6 + 2 + (itsVarDescriptors.size() * 10);
        }

        int attrLength = 2 +                    // attribute_name_index
                         4 +                    // attribute_length
                         2 +                    // max_stack
                         2 +                    // max_locals
                         4 +                    // code_length
                         itsCodeBufferTop +
                         2 +                    // exception_table_length
                         (itsExceptionTableTop * 8) +
                         2 +                    // attributes_count
                         lineNumberTableLength +
                         variableTableLength;

        byte[] codeAttribute = new byte[attrLength];
        int index = 0;
        int codeAttrIndex = itsConstantPool.addUtf8("Code");
        index = putInt16(codeAttrIndex, codeAttribute, index);
        attrLength -= 6;                 // discount the attribute header
        index = putInt32(attrLength, codeAttribute, index);
        index = putInt16(itsMaxStack, codeAttribute, index);
        index = putInt16(itsMaxLocals, codeAttribute, index);
        index = putInt32(itsCodeBufferTop, codeAttribute, index);
        System.arraycopy(itsCodeBuffer, 0, codeAttribute, index,
                         itsCodeBufferTop);
        index += itsCodeBufferTop;


        if (itsExceptionTableTop > 0) {
            index = putInt16(itsExceptionTableTop, codeAttribute, index);
            for (int i = 0; i < itsExceptionTableTop; i++) {
                ExceptionTableEntry ete = itsExceptionTable[i];
                short startPC = (short)getLabelPC(ete.itsStartLabel);
                short endPC = (short)getLabelPC(ete.itsEndLabel);
                short handlerPC = (short)getLabelPC(ete.itsHandlerLabel);
                short catchType = ete.itsCatchType;
                if (startPC == -1)
                    throw new IllegalStateException("start label not defined");
                if (endPC == -1)
                    throw new IllegalStateException("end label not defined");
                if (handlerPC == -1)
                    throw new IllegalStateException(
                        "handler label not defined");

                index = putInt16(startPC, codeAttribute, index);
                index = putInt16(endPC, codeAttribute, index);
                index = putInt16(handlerPC, codeAttribute, index);
                index = putInt16(catchType, codeAttribute, index);
            }
        }
        else {
            // write 0 as exception table length
            index = putInt16(0, codeAttribute, index);
        }

        int attributeCount = 0;
        if (itsLineNumberTable != null)
            attributeCount++;
        if (itsVarDescriptors != null)
            attributeCount++;
        index = putInt16(attributeCount, codeAttribute, index);

        if (itsLineNumberTable != null) {
            int lineNumberTableAttrIndex
                    = itsConstantPool.addUtf8("LineNumberTable");
            index = putInt16(lineNumberTableAttrIndex, codeAttribute, index);
            int tableAttrLength = 2 + (itsLineNumberTableTop * 4);
            index = putInt32(tableAttrLength, codeAttribute, index);
            index = putInt16(itsLineNumberTableTop, codeAttribute, index);
            for (int i = 0; i < itsLineNumberTableTop; i++) {
                index = putInt32(itsLineNumberTable[i], codeAttribute, index);
            }
        }

        if (itsVarDescriptors != null) {
            int variableTableAttrIndex
                    = itsConstantPool.addUtf8("LocalVariableTable");
            index = putInt16(variableTableAttrIndex, codeAttribute, index);
            int varCount = itsVarDescriptors.size();
            int tableAttrLength = 2 + (varCount * 10);
            index = putInt32(tableAttrLength, codeAttribute, index);
            index = putInt16(varCount, codeAttribute, index);
            for (int i = 0; i < varCount; i++) {
                int[] chunk = (int[])itsVarDescriptors.get(i);
                int nameIndex       = chunk[0];
                int descriptorIndex = chunk[1];
                int startPC         = chunk[2];
                int register        = chunk[3];
                int length = itsCodeBufferTop - startPC;

                index = putInt16(startPC, codeAttribute, index);
                index = putInt16(length, codeAttribute, index);
                index = putInt16(nameIndex, codeAttribute, index);
                index = putInt16(descriptorIndex, codeAttribute, index);
                index = putInt16(register, codeAttribute, index);
            }
        }

        itsCurrentMethod.setCodeAttribute(codeAttribute);

        itsExceptionTable = null;
        itsExceptionTableTop = 0;
        itsLineNumberTableTop = 0;
        itsCodeBufferTop = 0;
        itsCurrentMethod = null;
        itsMaxStack = 0;
        itsStackTop = 0;
        itsLabelTableTop = 0;
        itsFixupTableTop = 0;
        itsVarDescriptors = null;
    }

    /**
     * Add the single-byte opcode to the current method.
     *
     * @param theOpCode the opcode of the bytecode
     */
    public void add(int theOpCode) {
        if (opcodeCount(theOpCode) != 0)
            throw new IllegalArgumentException("Unexpected operands");
        int newStack = itsStackTop + stackChange(theOpCode);
        if (newStack < 0 || Short.MAX_VALUE < newStack) badStack(newStack);
        if (DEBUGCODE)
            System.out.println("Add " + bytecodeStr(theOpCode));
        addToCodeBuffer(theOpCode);
        itsStackTop = (short)newStack;
        if (newStack > itsMaxStack) itsMaxStack = (short)newStack;
        if (DEBUGSTACK) {
            System.out.println("After "+bytecodeStr(theOpCode)
                               +" stack = "+itsStackTop);
        }
    }

    /**
     * Add a single-operand opcode to the current method.
     *
     * @param theOpCode the opcode of the bytecode
     * @param theOperand the operand of the bytecode
     */
    public void add(int theOpCode, int theOperand) {
        if (DEBUGCODE) {
            System.out.println("Add "+bytecodeStr(theOpCode)
                               +", "+Integer.toHexString(theOperand));
        }
        int newStack = itsStackTop + stackChange(theOpCode);
        if (newStack < 0 || Short.MAX_VALUE < newStack) badStack(newStack);

        switch (theOpCode) {
            case ByteCode.GOTO :
                // fallthru...
            case ByteCode.IFEQ :
            case ByteCode.IFNE :
            case ByteCode.IFLT :
            case ByteCode.IFGE :
            case ByteCode.IFGT :
            case ByteCode.IFLE :
            case ByteCode.IF_ICMPEQ :
            case ByteCode.IF_ICMPNE :
            case ByteCode.IF_ICMPLT :
            case ByteCode.IF_ICMPGE :
            case ByteCode.IF_ICMPGT :
            case ByteCode.IF_ICMPLE :
            case ByteCode.IF_ACMPEQ :
            case ByteCode.IF_ACMPNE :
            case ByteCode.JSR :
            case ByteCode.IFNULL :
            case ByteCode.IFNONNULL : {
                    if ((theOperand & 0x80000000) != 0x80000000) {
                        if ((theOperand < 0) || (theOperand > 65535))
                            throw new IllegalArgumentException(
                                "Bad label for branch");
                    }
                    int branchPC = itsCodeBufferTop;
                    addToCodeBuffer(theOpCode);
                    if ((theOperand & 0x80000000) != 0x80000000) {
                            // hard displacement
                        addToCodeInt16(theOperand);
                    }
                    else {  // a label
                        int targetPC = getLabelPC(theOperand);
                        if (DEBUGLABELS) {
                            int theLabel = theOperand & 0x7FFFFFFF;
                            System.out.println("Fixing branch to " +
                                               theLabel + " at " + targetPC +
                                               " from " + branchPC);
                        }
                        if (targetPC != -1) {
                            int offset = targetPC - branchPC;
                            addToCodeInt16(offset);
                        }
                        else {
                            addLabelFixup(theOperand, branchPC + 1);
                            addToCodeInt16(0);
                        }
                    }
                }
                break;

            case ByteCode.BIPUSH :
                if ((byte)theOperand != theOperand)
                    throw new IllegalArgumentException("out of range byte");
                addToCodeBuffer(theOpCode);
                addToCodeBuffer((byte)theOperand);
                break;

            case ByteCode.SIPUSH :
                if ((short)theOperand != theOperand)
                    throw new IllegalArgumentException("out of range short");
                addToCodeBuffer(theOpCode);
                   addToCodeInt16(theOperand);
                break;

            case ByteCode.NEWARRAY :
                if (!(0 <= theOperand && theOperand < 256))
                    throw new IllegalArgumentException("out of range index");
                addToCodeBuffer(theOpCode);
                addToCodeBuffer(theOperand);
                break;

            case ByteCode.GETFIELD :
            case ByteCode.PUTFIELD :
                if (!(0 <= theOperand && theOperand < 65536))
                    throw new IllegalArgumentException("out of range field");
                addToCodeBuffer(theOpCode);
                addToCodeInt16(theOperand);
                break;

            case ByteCode.LDC :
            case ByteCode.LDC_W :
            case ByteCode.LDC2_W :
                if (!(0 <= theOperand && theOperand < 65536))
                    throw new IllegalArgumentException("out of range index");
                if (theOperand >= 256
                    || theOpCode == ByteCode.LDC_W
                    || theOpCode == ByteCode.LDC2_W)
                {
                    if (theOpCode == ByteCode.LDC) {
                        addToCodeBuffer(ByteCode.LDC_W);
                    } else {
                        addToCodeBuffer(theOpCode);
                    }
                    addToCodeInt16(theOperand);
                } else {
                    addToCodeBuffer(theOpCode);
                    addToCodeBuffer(theOperand);
                }
                break;

            case ByteCode.RET :
            case ByteCode.ILOAD :
            case ByteCode.LLOAD :
            case ByteCode.FLOAD :
            case ByteCode.DLOAD :
            case ByteCode.ALOAD :
            case ByteCode.ISTORE :
            case ByteCode.LSTORE :
            case ByteCode.FSTORE :
            case ByteCode.DSTORE :
            case ByteCode.ASTORE :
                if (!(0 <= theOperand && theOperand < 65536))
                    throw new IllegalArgumentException("out of range variable");
                if (theOperand >= 256) {
                    addToCodeBuffer(ByteCode.WIDE);
                    addToCodeBuffer(theOpCode);
                    addToCodeInt16(theOperand);
                }
                else {
                    addToCodeBuffer(theOpCode);
                    addToCodeBuffer(theOperand);
                }
                break;

            default :
                throw new IllegalArgumentException(
                    "Unexpected opcode for 1 operand");
        }

        itsStackTop = (short)newStack;
        if (newStack > itsMaxStack) itsMaxStack = (short)newStack;
        if (DEBUGSTACK) {
            System.out.println("After "+bytecodeStr(theOpCode)
                               +" stack = "+itsStackTop);
        }
    }

    /**
     * Generate the load constant bytecode for the given integer.
     *
     * @param k the constant
     */
    public void addLoadConstant(int k) {
        add(ByteCode.LDC, itsConstantPool.addConstant(k));
    }

    /**
     * Generate the load constant bytecode for the given long.
     *
     * @param k the constant
     */
    public void addLoadConstant(long k) {
        add(ByteCode.LDC2_W, itsConstantPool.addConstant(k));
    }

    /**
     * Generate the load constant bytecode for the given float.
     *
     * @param k the constant
     */
    public void addLoadConstant(float k) {
        add(ByteCode.LDC, itsConstantPool.addConstant(k));
    }

    /**
     * Generate the load constant bytecode for the given double.
     *
     * @param k the constant
     */
    public void addLoadConstant(double k) {
        add(ByteCode.LDC2_W, itsConstantPool.addConstant(k));
    }

    /**
     * Generate the load constant bytecode for the given string.
     *
     * @param k the constant
     */
    public void addLoadConstant(String k) {
        add(ByteCode.LDC, itsConstantPool.addConstant(k));
    }

    /**
     * Add the given two-operand bytecode to the current method.
     *
     * @param theOpCode the opcode of the bytecode
     * @param theOperand1 the first operand of the bytecode
     * @param theOperand2 the second operand of the bytecode
     */
    public void add(int theOpCode, int theOperand1, int theOperand2) {
        if (DEBUGCODE) {
            System.out.println("Add "+bytecodeStr(theOpCode)
                               +", "+Integer.toHexString(theOperand1)
                               +", "+Integer.toHexString(theOperand2));
        }
        int newStack = itsStackTop + stackChange(theOpCode);
        if (newStack < 0 || Short.MAX_VALUE < newStack) badStack(newStack);

        if (theOpCode == ByteCode.IINC) {
            if (!(0 <= theOperand1 && theOperand1 < 65536))
                throw new IllegalArgumentException("out of range variable");
            if (!(0 <= theOperand2 && theOperand2 < 65536))
                throw new IllegalArgumentException("out of range increment");

            if (theOperand1 > 255 || theOperand2 < -128 || theOperand2 > 127) {
                addToCodeBuffer(ByteCode.WIDE);
                addToCodeBuffer(ByteCode.IINC);
                addToCodeInt16(theOperand1);
                addToCodeInt16(theOperand2);
            }
            else {
                addToCodeBuffer(ByteCode.WIDE);
                addToCodeBuffer(ByteCode.IINC);
                addToCodeBuffer(theOperand1);
                addToCodeBuffer(theOperand2);
            }
        }
        else if (theOpCode == ByteCode.MULTIANEWARRAY) {
            if (!(0 <= theOperand1 && theOperand1 < 65536))
                throw new IllegalArgumentException("out of range index");
            if (!(0 <= theOperand2 && theOperand2 < 256))
                throw new IllegalArgumentException("out of range dimensions");

            addToCodeBuffer(ByteCode.MULTIANEWARRAY);
            addToCodeInt16(theOperand1);
            addToCodeBuffer(theOperand2);
        }
        else {
            throw new IllegalArgumentException(
                "Unexpected opcode for 2 operands");
        }
        itsStackTop = (short)newStack;
        if (newStack > itsMaxStack) itsMaxStack = (short)newStack;
        if (DEBUGSTACK) {
            System.out.println("After "+bytecodeStr(theOpCode)
                               +" stack = "+itsStackTop);
        }

    }

    public void add(int theOpCode, String className) {
        if (DEBUGCODE) {
            System.out.println("Add "+bytecodeStr(theOpCode)
                               +", "+className);
        }
        int newStack = itsStackTop + stackChange(theOpCode);
        if (newStack < 0 || Short.MAX_VALUE < newStack) badStack(newStack);
        switch (theOpCode) {
            case ByteCode.NEW :
            case ByteCode.ANEWARRAY :
            case ByteCode.CHECKCAST :
            case ByteCode.INSTANCEOF : {
                short classIndex = itsConstantPool.addClass(className);
                addToCodeBuffer(theOpCode);
                addToCodeInt16(classIndex);
            }
            break;

            default :
                throw new IllegalArgumentException(
                    "bad opcode for class reference");
        }
        itsStackTop = (short)newStack;
        if (newStack > itsMaxStack) itsMaxStack = (short)newStack;
        if (DEBUGSTACK) {
            System.out.println("After "+bytecodeStr(theOpCode)
                               +" stack = "+itsStackTop);
        }
    }


    public void add(int theOpCode, String className, String fieldName,
                    String fieldType)
    {
        if (DEBUGCODE) {
            System.out.println("Add "+bytecodeStr(theOpCode)
                               +", "+className+", "+fieldName+", "+fieldType);
        }
        int newStack = itsStackTop + stackChange(theOpCode);
        char fieldTypeChar = fieldType.charAt(0);
        int fieldSize = (fieldTypeChar == 'J' || fieldTypeChar == 'D')
                        ? 2 : 1;
        switch (theOpCode) {
            case ByteCode.GETFIELD :
            case ByteCode.GETSTATIC :
                newStack += fieldSize;
                break;
            case ByteCode.PUTSTATIC :
            case ByteCode.PUTFIELD :
                newStack -= fieldSize;
                break;
            default :
                throw new IllegalArgumentException(
                    "bad opcode for field reference");
        }
        if (newStack < 0 || Short.MAX_VALUE < newStack) badStack(newStack);
        short fieldRefIndex = itsConstantPool.addFieldRef(className,
                                             fieldName, fieldType);
        addToCodeBuffer(theOpCode);
        addToCodeInt16(fieldRefIndex);

        itsStackTop = (short)newStack;
        if (newStack > itsMaxStack) itsMaxStack = (short)newStack;
        if (DEBUGSTACK) {
            System.out.println("After "+bytecodeStr(theOpCode)
                               +" stack = "+itsStackTop);
        }
    }

    public void addInvoke(int theOpCode, String className, String methodName,
                          String methodType)
    {
        if (DEBUGCODE) {
            System.out.println("Add "+bytecodeStr(theOpCode)
                               +", "+className+", "+methodName+", "
                               +methodType);
        }
        int parameterInfo = sizeOfParameters(methodType);
        int parameterCount = parameterInfo >>> 16;
        int stackDiff = (short)parameterInfo;

        int newStack = itsStackTop + stackDiff;
        newStack += stackChange(theOpCode);     // adjusts for 'this'
        if (newStack < 0 || Short.MAX_VALUE < newStack) badStack(newStack);

        switch (theOpCode) {
            case ByteCode.INVOKEVIRTUAL :
            case ByteCode.INVOKESPECIAL :
            case ByteCode.INVOKESTATIC :
            case ByteCode.INVOKEINTERFACE : {
                    addToCodeBuffer(theOpCode);
                    if (theOpCode == ByteCode.INVOKEINTERFACE) {
                        short ifMethodRefIndex
                                    = itsConstantPool.addInterfaceMethodRef(
                                               className, methodName,
                                               methodType);
                        addToCodeInt16(ifMethodRefIndex);
                        addToCodeBuffer(parameterCount + 1);
                        addToCodeBuffer(0);
                    }
                    else {
                        short methodRefIndex = itsConstantPool.addMethodRef(
                                               className, methodName,
                                               methodType);
                        addToCodeInt16(methodRefIndex);
                    }
                }
                break;

            default :
                throw new IllegalArgumentException(
                    "bad opcode for method reference");
        }
        itsStackTop = (short)newStack;
        if (newStack > itsMaxStack) itsMaxStack = (short)newStack;
        if (DEBUGSTACK) {
            System.out.println("After "+bytecodeStr(theOpCode)
                               +" stack = "+itsStackTop);
        }
    }

    /**
     * Generate code to load the given integer on stack.
     *
     * @param k the constant
     */
    public void addPush(int k)
    {
        if ((byte)k == k) {
            if (k == -1) {
                add(ByteCode.ICONST_M1);
            } else if (0 <= k && k <= 5) {
                add((byte)(ByteCode.ICONST_0 + k));
            } else {
                add(ByteCode.BIPUSH, (byte)k);
            }
        } else if ((short)k == k) {
            add(ByteCode.SIPUSH, (short)k);
        } else {
            addLoadConstant(k);
        }
    }

    public void addPush(boolean k)
    {
        add(k ? ByteCode.ICONST_1 : ByteCode.ICONST_0);
    }

    /**
     * Generate code to load the given long on stack.
     *
     * @param k the constant
     */
    public void addPush(long k)
    {
        int ik = (int)k;
        if (ik == k) {
            addPush(ik);
            add(ByteCode.I2L);
        } else {
            addLoadConstant(k);
        }
    }

    /**
     * Generate code to load the given double on stack.
     *
     * @param k the constant
     */
    public void addPush(double k)
    {
        if (k == 0.0) {
            // zero
            add(ByteCode.DCONST_0);
            if (1.0 / k < 0) {
                // Negative zero
                add(ByteCode.DNEG);
            }
        } else if (k == 1.0 || k == -1.0) {
            add(ByteCode.DCONST_1);
            if (k < 0) {
                add(ByteCode.DNEG);
            }
        } else {
            addLoadConstant(k);
        }
    }

    /**
     * Generate the code to leave on stack the given string even if the
     * string encoding exeeds the class file limit for single string constant
     *
     * @param k the constant
     */
    public void addPush(String k) {
        int length = k.length();
        int limit = itsConstantPool.getUtfEncodingLimit(k, 0, length);
        if (limit == length) {
            addLoadConstant(k);
            return;
        }
        // Split string into picies fitting the UTF limit and generate code for
        // StringBuffer sb = new StringBuffer(length);
        // sb.append(loadConstant(piece_1));
        // ...
        // sb.append(loadConstant(piece_N));
        // sb.toString();
        final String SB = "java/lang/StringBuffer";
        add(ByteCode.NEW, SB);
        add(ByteCode.DUP);
        addPush(length);
        addInvoke(ByteCode.INVOKESPECIAL, SB, "<init>", "(I)V");
        int cursor = 0;
        for (;;) {
            add(ByteCode.DUP);
            String s = k.substring(cursor, limit);
            addLoadConstant(s);
            addInvoke(ByteCode.INVOKEVIRTUAL, SB, "append",
                      "(Ljava/lang/String;)Ljava/lang/StringBuffer;");
            add(ByteCode.POP);
            if (limit == length) {
                break;
            }
            cursor = limit;
            limit = itsConstantPool.getUtfEncodingLimit(k, limit, length);
        }
        addInvoke(ByteCode.INVOKEVIRTUAL, SB, "toString",
                  "()Ljava/lang/String;");
    }

    /**
     * Check if k fits limit on string constant size imposed by class file
     * format.
     *
     * @param k the string constant
     */
    public boolean isUnderStringSizeLimit(String k)
    {
        return itsConstantPool.isUnderUtfEncodingLimit(k);
    }

    /**
     * Store integer from stack top into the given local.
     *
     * @param local number of local register
     */
    public void addIStore(int local)
    {
        xop(ByteCode.ISTORE_0, ByteCode.ISTORE, local);
    }

    /**
     * Store long from stack top into the given local.
     *
     * @param local number of local register
     */
    public void addLStore(int local)
    {
        xop(ByteCode.LSTORE_0, ByteCode.LSTORE, local);
    }

    /**
     * Store float from stack top into the given local.
     *
     * @param local number of local register
     */
    public void addFStore(int local)
    {
        xop(ByteCode.FSTORE_0, ByteCode.FSTORE, local);
    }

    /**
     * Store double from stack top into the given local.
     *
     * @param local number of local register
     */
    public void addDStore(int local)
    {
        xop(ByteCode.DSTORE_0, ByteCode.DSTORE, local);
    }

    /**
     * Store object from stack top into the given local.
     *
     * @param local number of local register
     */
    public void addAStore(int local)
    {
        xop(ByteCode.ASTORE_0, ByteCode.ASTORE, local);
    }

    /**
     * Load integer from the given local into stack.
     *
     * @param local number of local register
     */
    public void addILoad(int local)
    {
        xop(ByteCode.ILOAD_0, ByteCode.ILOAD, local);
    }

    /**
     * Load long from the given local into stack.
     *
     * @param local number of local register
     */
    public void addLLoad(int local)
    {
        xop(ByteCode.LLOAD_0, ByteCode.LLOAD, local);
    }

    /**
     * Load float from the given local into stack.
     *
     * @param local number of local register
     */
    public void addFLoad(int local)
    {
        xop(ByteCode.FLOAD_0, ByteCode.FLOAD, local);
    }

    /**
     * Load double from the given local into stack.
     *
     * @param local number of local register
     */
    public void addDLoad(int local)
    {
        xop(ByteCode.DLOAD_0, ByteCode.DLOAD, local);
    }

    /**
     * Load object from the given local into stack.
     *
     * @param local number of local register
     */
    public void addALoad(int local)
    {
        xop(ByteCode.ALOAD_0, ByteCode.ALOAD, local);
    }

    /**
     * Load "this" into stack.
     */
    public void addLoadThis()
    {
        add(ByteCode.ALOAD_0);
    }

    private void xop(int shortOp, int op, int local)
    {
        switch (local) {
          case 0:
            add(shortOp);
            break;
          case 1:
            add(shortOp + 1);
            break;
          case 2:
            add(shortOp + 2);
            break;
          case 3:
            add(shortOp + 3);
            break;
          default:
            add(op, local);
        }
    }

    public int addTableSwitch(int low, int high)
    {
        if (DEBUGCODE) {
            System.out.println("Add "+bytecodeStr(ByteCode.TABLESWITCH)
                               +" "+low+" "+high);
        }
        if (low > high)
            throw new IllegalArgumentException("Bad bounds: "+low+' '+ high);

        int newStack = itsStackTop + stackChange(ByteCode.TABLESWITCH);
        if (newStack < 0 || Short.MAX_VALUE < newStack) badStack(newStack);

        int entryCount = high - low + 1;
        int padSize = 3 & ~itsCodeBufferTop; // == 3 - itsCodeBufferTop % 4

        int N = addReservedCodeSpace(1 + padSize + 4 * (1 + 2 + entryCount));
        int switchStart = N;
        itsCodeBuffer[N++] = (byte)ByteCode.TABLESWITCH;
        while (padSize != 0) {
            itsCodeBuffer[N++] = 0;
            --padSize;
        }
        N += 4; // skip default offset
        N = putInt32(low, itsCodeBuffer, N);
        putInt32(high, itsCodeBuffer, N);

        itsStackTop = (short)newStack;
        if (newStack > itsMaxStack) itsMaxStack = (short)newStack;
        if (DEBUGSTACK) {
            System.out.println("After "+bytecodeStr(ByteCode.TABLESWITCH)
                               +" stack = "+itsStackTop);
        }

        return switchStart;
    }

    public final void markTableSwitchDefault(int switchStart)
    {
        setTableSwitchJump(switchStart, -1, itsCodeBufferTop);
    }

    public final void markTableSwitchCase(int switchStart, int caseIndex)
    {
        setTableSwitchJump(switchStart, caseIndex, itsCodeBufferTop);
    }

    public final void markTableSwitchCase(int switchStart, int caseIndex,
                                          int stackTop)
    {
        if (!(0 <= stackTop && stackTop <= itsMaxStack))
            throw new IllegalArgumentException("Bad stack index: "+stackTop);
        itsStackTop = (short)stackTop;
        setTableSwitchJump(switchStart, caseIndex, itsCodeBufferTop);
    }

    public void setTableSwitchJump(int switchStart, int caseIndex,
                                   int jumpTarget)
    {
        if (!(0 <= jumpTarget && jumpTarget <= itsCodeBufferTop))
            throw new IllegalArgumentException("Bad jump target: "+jumpTarget);
        if (!(caseIndex >= -1))
            throw new IllegalArgumentException("Bad case index: "+caseIndex);

        int padSize = 3 & ~switchStart; // == 3 - switchStart % 4
        int caseOffset;
        if (caseIndex < 0) {
            // default label
            caseOffset = switchStart + 1 + padSize;
        } else {
            caseOffset = switchStart + 1 + padSize + 4 * (3 + caseIndex);
        }
        if (!(0 <= switchStart
              && switchStart <= itsCodeBufferTop - 4 * 4 - padSize - 1))
        {
            throw new IllegalArgumentException(
                switchStart+" is outside a possible range of tableswitch"
                +" in already generated code");
        }
        if ((0xFF & itsCodeBuffer[switchStart]) != ByteCode.TABLESWITCH) {
            throw new IllegalArgumentException(
                switchStart+" is not offset of tableswitch statement");
        }
        if (!(0 <= caseOffset && caseOffset + 4 <= itsCodeBufferTop)) {
            // caseIndex >= -1 does not guarantee that caseOffset >= 0 due
            // to a possible overflow.
            throw new IllegalArgumentException(
                "Too big case index: "+caseIndex);
        }
        // ALERT: perhaps check against case bounds?
        putInt32(jumpTarget - switchStart, itsCodeBuffer, caseOffset);
    }

    public int acquireLabel()
    {
        int top = itsLabelTableTop;
        if (itsLabelTable == null || top == itsLabelTable.length) {
            if (itsLabelTable == null) {
                itsLabelTable = new int[MIN_LABEL_TABLE_SIZE];
            }else {
                int[] tmp = new int[itsLabelTable.length * 2];
                System.arraycopy(itsLabelTable, 0, tmp, 0, top);
                itsLabelTable = tmp;
            }
        }
        itsLabelTableTop = top + 1;
        itsLabelTable[top] = -1;
        return top | 0x80000000;
    }

    public void markLabel(int label)
    {
        if (!(label < 0))
            throw new IllegalArgumentException("Bad label, no biscuit");

        label &= 0x7FFFFFFF;
        if (label > itsLabelTableTop)
            throw new IllegalArgumentException("Bad label");

        if (itsLabelTable[label] != -1) {
            throw new IllegalStateException("Can only mark label once");
        }

        itsLabelTable[label] = itsCodeBufferTop;
    }

    public void markLabel(int label, short stackTop)
    {
        markLabel(label);
        itsStackTop = stackTop;
    }

    public void markHandler(int theLabel) {
        itsStackTop = 1;
        markLabel(theLabel);
    }

    private int getLabelPC(int label)
    {
        if (!(label < 0))
            throw new IllegalArgumentException("Bad label, no biscuit");
        label &= 0x7FFFFFFF;
        if (!(label < itsLabelTableTop))
            throw new IllegalArgumentException("Bad label");
        return itsLabelTable[label];
    }

    private void addLabelFixup(int label, int fixupSite)
    {
        if (!(label < 0))
            throw new IllegalArgumentException("Bad label, no biscuit");
        label &= 0x7FFFFFFF;
        if (!(label < itsLabelTableTop))
            throw new IllegalArgumentException("Bad label");
        int top = itsFixupTableTop;
        if (itsFixupTable == null || top == itsFixupTable.length) {
            if (itsFixupTable == null) {
                itsFixupTable = new long[MIN_FIXUP_TABLE_SIZE];
            }else {
                long[] tmp = new long[itsFixupTable.length * 2];
                System.arraycopy(itsFixupTable, 0, tmp, 0, top);
                itsFixupTable = tmp;
            }
        }
        itsFixupTableTop = top + 1;
        itsFixupTable[top] = ((long)label << 32) | fixupSite;
    }

    private  void fixLabelGotos()
    {
        byte[] codeBuffer = itsCodeBuffer;
        for (int i = 0; i < itsFixupTableTop; i++) {
            long fixup = itsFixupTable[i];
            int label = (int)(fixup >> 32);
            int fixupSite = (int)fixup;
            int pc = itsLabelTable[label];
            if (pc == -1) {
                // Unlocated label
                throw new RuntimeException();
            }
            // -1 to get delta from instruction start
            int offset = pc - (fixupSite - 1);
            if ((short)offset != offset) {
                throw new RuntimeException
                    ("Program too complex: too big jump offset");
            }
            codeBuffer[fixupSite] = (byte)(offset >> 8);
            codeBuffer[fixupSite + 1] = (byte)offset;
        }
        itsFixupTableTop = 0;
    }

    /**
     * Get the current offset into the code of the current method.
     *
     * @return an integer representing the offset
     */
    public int getCurrentCodeOffset() {
        return itsCodeBufferTop;
    }

    public short getStackTop() {
        return itsStackTop;
    }

    public void adjustStackTop(int delta) {
        int newStack = itsStackTop + delta;
        if (newStack < 0 || Short.MAX_VALUE < newStack) badStack(newStack);
        itsStackTop = (short)newStack;
        if (newStack > itsMaxStack) itsMaxStack = (short)newStack;
        if (DEBUGSTACK) {
            System.out.println("After "+"adjustStackTop("+delta+")"
                               +" stack = "+itsStackTop);
        }
    }

    private void addToCodeBuffer(int b)
    {
        int N = addReservedCodeSpace(1);
        itsCodeBuffer[N] = (byte)b;
    }

    private void addToCodeInt16(int value)
    {
        int N = addReservedCodeSpace(2);
        putInt16(value, itsCodeBuffer, N);
    }

    private int addReservedCodeSpace(int size)
    {
        if (itsCurrentMethod == null)
            throw new IllegalArgumentException("No method to add to");
        int oldTop = itsCodeBufferTop;
        int newTop = oldTop + size;
        if (newTop > itsCodeBuffer.length) {
            int newSize = itsCodeBuffer.length * 2;
            if (newTop > newSize) { newSize = newTop; }
            byte[] tmp = new byte[newSize];
            System.arraycopy(itsCodeBuffer, 0, tmp, 0, oldTop);
            itsCodeBuffer = tmp;
        }
        itsCodeBufferTop = newTop;
        return oldTop;
    }

    public void addExceptionHandler(int startLabel, int endLabel,
                                    int handlerLabel, String catchClassName)
    {
        if ((startLabel & 0x80000000) != 0x80000000)
            throw new IllegalArgumentException("Bad startLabel");
        if ((endLabel & 0x80000000) != 0x80000000)
            throw new IllegalArgumentException("Bad endLabel");
        if ((handlerLabel & 0x80000000) != 0x80000000)
            throw new IllegalArgumentException("Bad handlerLabel");

        /*
         * If catchClassName is null, use 0 for the catch_type_index; which
         * means catch everything.  (Even when the verifier has let you throw
         * something other than a Throwable.)
         */
        short catch_type_index = (catchClassName == null)
                                 ? 0
                                 : itsConstantPool.addClass(catchClassName);
        ExceptionTableEntry newEntry = new ExceptionTableEntry(
                                           startLabel,
                                           endLabel,
                                           handlerLabel,
                                           catch_type_index);
        int N = itsExceptionTableTop;
        if (N == 0) {
            itsExceptionTable = new ExceptionTableEntry[ExceptionTableSize];
        } else if (N == itsExceptionTable.length) {
            ExceptionTableEntry[] tmp = new ExceptionTableEntry[N * 2];
            System.arraycopy(itsExceptionTable, 0, tmp, 0, N);
            itsExceptionTable = tmp;
        }
        itsExceptionTable[N] = newEntry;
        itsExceptionTableTop = N + 1;

    }

    public void addLineNumberEntry(short lineNumber) {
        if (itsCurrentMethod == null)
            throw new IllegalArgumentException("No method to stop");
        int N = itsLineNumberTableTop;
        if (N == 0) {
            itsLineNumberTable = new int[LineNumberTableSize];
        } else if (N == itsLineNumberTable.length) {
            int[] tmp = new int[N * 2];
            System.arraycopy(itsLineNumberTable, 0, tmp, 0, N);
            itsLineNumberTable = tmp;
        }
        itsLineNumberTable[N] = (itsCodeBufferTop << 16) + lineNumber;
        itsLineNumberTableTop = N + 1;
    }

    /**
     * Write the class file to the OutputStream.
     *
     * @param oStream the stream to write to
     * @throws IOException if writing to the stream produces an exception
     */
    public void write(OutputStream oStream)
        throws IOException
    {
        byte[] array = toByteArray();
        oStream.write(array);
    }

    private int getWriteSize()
    {
        int size = 0;

        if (itsSourceFileNameIndex != 0) {
            itsConstantPool.addUtf8("SourceFile");
        }

        size += 8; //writeLong(FileHeaderConstant);
        size += itsConstantPool.getWriteSize();
        size += 2; //writeShort(itsFlags);
        size += 2; //writeShort(itsThisClassIndex);
        size += 2; //writeShort(itsSuperClassIndex);
        size += 2; //writeShort(itsInterfaces.size());
        size += 2 * itsInterfaces.size();

        size += 2; //writeShort(itsFields.size());
        for (int i = 0; i < itsFields.size(); i++) {
            size += ((ClassFileField)(itsFields.get(i))).getWriteSize();
        }

        size += 2; //writeShort(itsMethods.size());
        for (int i = 0; i < itsMethods.size(); i++) {
            size += ((ClassFileMethod)(itsMethods.get(i))).getWriteSize();
        }

        if (itsSourceFileNameIndex != 0) {
            size += 2; //writeShort(1);  attributes count
            size += 2; //writeShort(sourceFileAttributeNameIndex);
            size += 4; //writeInt(2);
            size += 2; //writeShort(itsSourceFileNameIndex);
        }else {
            size += 2; //out.writeShort(0);  no attributes
        }

        return size;
    }

    /**
     * Get the class file as array of bytesto the OutputStream.
     */
    public byte[] toByteArray()
    {
        int dataSize = getWriteSize();
        byte[] data = new byte[dataSize];
        int offset = 0;

        short sourceFileAttributeNameIndex = 0;
        if (itsSourceFileNameIndex != 0) {
            sourceFileAttributeNameIndex = itsConstantPool.addUtf8(
                                               "SourceFile");
        }

        offset = putInt64(FileHeaderConstant, data, offset);
        offset = itsConstantPool.write(data, offset);
        offset = putInt16(itsFlags, data, offset);
        offset = putInt16(itsThisClassIndex, data, offset);
        offset = putInt16(itsSuperClassIndex, data, offset);
        offset = putInt16(itsInterfaces.size(), data, offset);
        for (int i = 0; i < itsInterfaces.size(); i++) {
            int interfaceIndex = ((Short)(itsInterfaces.get(i))).shortValue();
            offset = putInt16(interfaceIndex, data, offset);
        }
        offset = putInt16(itsFields.size(), data, offset);
        for (int i = 0; i < itsFields.size(); i++) {
            ClassFileField field = (ClassFileField)itsFields.get(i);
            offset = field.write(data, offset);
        }
        offset = putInt16(itsMethods.size(), data, offset);
        for (int i = 0; i < itsMethods.size(); i++) {
            ClassFileMethod method = (ClassFileMethod)itsMethods.get(i);
            offset = method.write(data, offset);
        }
        if (itsSourceFileNameIndex != 0) {
            offset = putInt16(1, data, offset); // attributes count
            offset = putInt16(sourceFileAttributeNameIndex, data, offset);
            offset = putInt32(2, data, offset);
            offset = putInt16(itsSourceFileNameIndex, data, offset);
        } else {
            offset = putInt16(0, data, offset); // no attributes
        }

        if (offset != dataSize) {
            // Check getWriteSize is consistent with write!
            throw new RuntimeException();
        }

        return data;
    }

    static int putInt64(long value, byte[] array, int offset)
    {
        offset = putInt32((int)(value >>> 32), array, offset);
        return putInt32((int)value, array, offset);
    }

    private static void badStack(int value)
    {
        String s;
        if (value < 0) { s = "Stack underflow: "+value; }
        else { s = "Too big stack: "+value; }
        throw new IllegalStateException(s);
    }

    /*
        Really weird. Returns an int with # parameters in hi 16 bits, and
        stack difference removal of parameters from stack and pushing the
        result (it does not take into account removal of this in case of
        non-static methods).
        If Java really supported references we wouldn't have to be this
        perverted.
    */
    private static int sizeOfParameters(String pString)
    {
        int length = pString.length();
        int rightParenthesis = pString.lastIndexOf(')');
        if (3 <= length /* minimal signature takes at least 3 chars: ()V */
            && pString.charAt(0) == '('
            && 1 <= rightParenthesis && rightParenthesis + 1 < length)
        {
            boolean ok = true;
            int index = 1;
            int stackDiff = 0;
            int count = 0;
        stringLoop:
            while (index != rightParenthesis) {
                switch (pString.charAt(index)) {
                    default:
                        ok = false;
                        break stringLoop;
                    case 'J' :
                    case 'D' :
                        --stackDiff;
                        // fall thru
                    case 'B' :
                    case 'S' :
                    case 'C' :
                    case 'I' :
                    case 'Z' :
                    case 'F' :
                        --stackDiff;
                        ++count;
                        ++index;
                        continue;
                    case '[' :
                        ++index;
                        int c = pString.charAt(index);
                        while (c == '[') {
                            ++index;
                            c = pString.charAt(index);
                        }
                        switch (c) {
                            default:
                                ok = false;
                                break stringLoop;
                            case 'J' :
                            case 'D' :
                            case 'B' :
                            case 'S' :
                            case 'C' :
                            case 'I' :
                            case 'Z' :
                            case 'F' :
                                --stackDiff;
                                ++count;
                                ++index;
                                continue;
                            case 'L':
                                // fall thru
                        }
                          // fall thru
                    case 'L' : {
                        --stackDiff;
                        ++count;
                        ++index;
                        int semicolon = pString.indexOf(';',  index);
                        if (!(index + 1 <= semicolon
                            && semicolon < rightParenthesis))
                        {
                            ok = false;
                            break stringLoop;
                        }
                        index = semicolon + 1;
                        continue;
                    }
                }
            }
            if (ok) {
                switch (pString.charAt(rightParenthesis + 1)) {
                    default:
                        ok = false;
                        break;
                    case 'J' :
                    case 'D' :
                        ++stackDiff;
                        // fall thru
                    case 'B' :
                    case 'S' :
                    case 'C' :
                    case 'I' :
                    case 'Z' :
                    case 'F' :
                    case 'L' :
                    case '[' :
                        ++stackDiff;
                        // fall thru
                    case 'V' :
                        break;
                }
                if (ok) {
                    return ((count << 16) | (0xFFFF & stackDiff));
                }
            }
        }
        throw new IllegalArgumentException(
            "Bad parameter signature: "+pString);
    }

    static int putInt16(int value, byte[] array, int offset)
    {
        array[offset + 0] = (byte)(value >>> 8);
        array[offset + 1] = (byte)value;
        return offset + 2;
    }

    static int putInt32(int value, byte[] array, int offset)
    {
        array[offset + 0] = (byte)(value >>> 24);
        array[offset + 1] = (byte)(value >>> 16);
        array[offset + 2] = (byte)(value >>> 8);
        array[offset + 3] = (byte)value;
        return offset + 4;
    }

    /**
     * Number of operands accompanying the opcode.
     */
    static int opcodeCount(int opcode)
    {
        switch (opcode) {
            case ByteCode.AALOAD:
            case ByteCode.AASTORE:
            case ByteCode.ACONST_NULL:
            case ByteCode.ALOAD_0:
            case ByteCode.ALOAD_1:
            case ByteCode.ALOAD_2:
            case ByteCode.ALOAD_3:
            case ByteCode.ARETURN:
            case ByteCode.ARRAYLENGTH:
            case ByteCode.ASTORE_0:
            case ByteCode.ASTORE_1:
            case ByteCode.ASTORE_2:
            case ByteCode.ASTORE_3:
            case ByteCode.ATHROW:
            case ByteCode.BALOAD:
            case ByteCode.BASTORE:
            case ByteCode.BREAKPOINT:
            case ByteCode.CALOAD:
            case ByteCode.CASTORE:
            case ByteCode.D2F:
            case ByteCode.D2I:
            case ByteCode.D2L:
            case ByteCode.DADD:
            case ByteCode.DALOAD:
            case ByteCode.DASTORE:
            case ByteCode.DCMPG:
            case ByteCode.DCMPL:
            case ByteCode.DCONST_0:
            case ByteCode.DCONST_1:
            case ByteCode.DDIV:
            case ByteCode.DLOAD_0:
            case ByteCode.DLOAD_1:
            case ByteCode.DLOAD_2:
            case ByteCode.DLOAD_3:
            case ByteCode.DMUL:
            case ByteCode.DNEG:
            case ByteCode.DREM:
            case ByteCode.DRETURN:
            case ByteCode.DSTORE_0:
            case ByteCode.DSTORE_1:
            case ByteCode.DSTORE_2:
            case ByteCode.DSTORE_3:
            case ByteCode.DSUB:
            case ByteCode.DUP:
            case ByteCode.DUP2:
            case ByteCode.DUP2_X1:
            case ByteCode.DUP2_X2:
            case ByteCode.DUP_X1:
            case ByteCode.DUP_X2:
            case ByteCode.F2D:
            case ByteCode.F2I:
            case ByteCode.F2L:
            case ByteCode.FADD:
            case ByteCode.FALOAD:
            case ByteCode.FASTORE:
            case ByteCode.FCMPG:
            case ByteCode.FCMPL:
            case ByteCode.FCONST_0:
            case ByteCode.FCONST_1:
            case ByteCode.FCONST_2:
            case ByteCode.FDIV:
            case ByteCode.FLOAD_0:
            case ByteCode.FLOAD_1:
            case ByteCode.FLOAD_2:
            case ByteCode.FLOAD_3:
            case ByteCode.FMUL:
            case ByteCode.FNEG:
            case ByteCode.FREM:
            case ByteCode.FRETURN:
            case ByteCode.FSTORE_0:
            case ByteCode.FSTORE_1:
            case ByteCode.FSTORE_2:
            case ByteCode.FSTORE_3:
            case ByteCode.FSUB:
            case ByteCode.I2B:
            case ByteCode.I2C:
            case ByteCode.I2D:
            case ByteCode.I2F:
            case ByteCode.I2L:
            case ByteCode.I2S:
            case ByteCode.IADD:
            case ByteCode.IALOAD:
            case ByteCode.IAND:
            case ByteCode.IASTORE:
            case ByteCode.ICONST_0:
            case ByteCode.ICONST_1:
            case ByteCode.ICONST_2:
            case ByteCode.ICONST_3:
            case ByteCode.ICONST_4:
            case ByteCode.ICONST_5:
            case ByteCode.ICONST_M1:
            case ByteCode.IDIV:
            case ByteCode.ILOAD_0:
            case ByteCode.ILOAD_1:
            case ByteCode.ILOAD_2:
            case ByteCode.ILOAD_3:
            case ByteCode.IMPDEP1:
            case ByteCode.IMPDEP2:
            case ByteCode.IMUL:
            case ByteCode.INEG:
            case ByteCode.IOR:
            case ByteCode.IREM:
            case ByteCode.IRETURN:
            case ByteCode.ISHL:
            case ByteCode.ISHR:
            case ByteCode.ISTORE_0:
            case ByteCode.ISTORE_1:
            case ByteCode.ISTORE_2:
            case ByteCode.ISTORE_3:
            case ByteCode.ISUB:
            case ByteCode.IUSHR:
            case ByteCode.IXOR:
            case ByteCode.L2D:
            case ByteCode.L2F:
            case ByteCode.L2I:
            case ByteCode.LADD:
            case ByteCode.LALOAD:
            case ByteCode.LAND:
            case ByteCode.LASTORE:
            case ByteCode.LCMP:
            case ByteCode.LCONST_0:
            case ByteCode.LCONST_1:
            case ByteCode.LDIV:
            case ByteCode.LLOAD_0:
            case ByteCode.LLOAD_1:
            case ByteCode.LLOAD_2:
            case ByteCode.LLOAD_3:
            case ByteCode.LMUL:
            case ByteCode.LNEG:
            case ByteCode.LOR:
            case ByteCode.LREM:
            case ByteCode.LRETURN:
            case ByteCode.LSHL:
            case ByteCode.LSHR:
            case ByteCode.LSTORE_0:
            case ByteCode.LSTORE_1:
            case ByteCode.LSTORE_2:
            case ByteCode.LSTORE_3:
            case ByteCode.LSUB:
            case ByteCode.LUSHR:
            case ByteCode.LXOR:
            case ByteCode.MONITORENTER:
            case ByteCode.MONITOREXIT:
            case ByteCode.NOP:
            case ByteCode.POP:
            case ByteCode.POP2:
            case ByteCode.RETURN:
            case ByteCode.SALOAD:
            case ByteCode.SASTORE:
            case ByteCode.SWAP:
            case ByteCode.WIDE:
                return 0;
            case ByteCode.ALOAD:
            case ByteCode.ANEWARRAY:
            case ByteCode.ASTORE:
            case ByteCode.BIPUSH:
            case ByteCode.CHECKCAST:
            case ByteCode.DLOAD:
            case ByteCode.DSTORE:
            case ByteCode.FLOAD:
            case ByteCode.FSTORE:
            case ByteCode.GETFIELD:
            case ByteCode.GETSTATIC:
            case ByteCode.GOTO:
            case ByteCode.GOTO_W:
            case ByteCode.IFEQ:
            case ByteCode.IFGE:
            case ByteCode.IFGT:
            case ByteCode.IFLE:
            case ByteCode.IFLT:
            case ByteCode.IFNE:
            case ByteCode.IFNONNULL:
            case ByteCode.IFNULL:
            case ByteCode.IF_ACMPEQ:
            case ByteCode.IF_ACMPNE:
            case ByteCode.IF_ICMPEQ:
            case ByteCode.IF_ICMPGE:
            case ByteCode.IF_ICMPGT:
            case ByteCode.IF_ICMPLE:
            case ByteCode.IF_ICMPLT:
            case ByteCode.IF_ICMPNE:
            case ByteCode.ILOAD:
            case ByteCode.INSTANCEOF:
            case ByteCode.INVOKEINTERFACE:
            case ByteCode.INVOKESPECIAL:
            case ByteCode.INVOKESTATIC:
            case ByteCode.INVOKEVIRTUAL:
            case ByteCode.ISTORE:
            case ByteCode.JSR:
            case ByteCode.JSR_W:
            case ByteCode.LDC:
            case ByteCode.LDC2_W:
            case ByteCode.LDC_W:
            case ByteCode.LLOAD:
            case ByteCode.LSTORE:
            case ByteCode.NEW:
            case ByteCode.NEWARRAY:
            case ByteCode.PUTFIELD:
            case ByteCode.PUTSTATIC:
            case ByteCode.RET:
            case ByteCode.SIPUSH:
                return 1;

            case ByteCode.IINC:
            case ByteCode.MULTIANEWARRAY:
                return 2;

            case ByteCode.LOOKUPSWITCH:
            case ByteCode.TABLESWITCH:
                return -1;
        }
        throw new IllegalArgumentException("Bad opcode: "+opcode);
    }

    /**
     *  The effect on the operand stack of a given opcode.
     */
    static int stackChange(int opcode)
    {
        // For INVOKE... accounts only for popping this (unless static),
        // ignoring parameters and return type
        switch (opcode) {
            case ByteCode.DASTORE:
            case ByteCode.LASTORE:
                return -4;

            case ByteCode.AASTORE:
            case ByteCode.BASTORE:
            case ByteCode.CASTORE:
            case ByteCode.DCMPG:
            case ByteCode.DCMPL:
            case ByteCode.FASTORE:
            case ByteCode.IASTORE:
            case ByteCode.LCMP:
            case ByteCode.SASTORE:
                return -3;

            case ByteCode.DADD:
            case ByteCode.DDIV:
            case ByteCode.DMUL:
            case ByteCode.DREM:
            case ByteCode.DRETURN:
            case ByteCode.DSTORE:
            case ByteCode.DSTORE_0:
            case ByteCode.DSTORE_1:
            case ByteCode.DSTORE_2:
            case ByteCode.DSTORE_3:
            case ByteCode.DSUB:
            case ByteCode.IF_ACMPEQ:
            case ByteCode.IF_ACMPNE:
            case ByteCode.IF_ICMPEQ:
            case ByteCode.IF_ICMPGE:
            case ByteCode.IF_ICMPGT:
            case ByteCode.IF_ICMPLE:
            case ByteCode.IF_ICMPLT:
            case ByteCode.IF_ICMPNE:
            case ByteCode.LADD:
            case ByteCode.LAND:
            case ByteCode.LDIV:
            case ByteCode.LMUL:
            case ByteCode.LOR:
            case ByteCode.LREM:
            case ByteCode.LRETURN:
            case ByteCode.LSTORE:
            case ByteCode.LSTORE_0:
            case ByteCode.LSTORE_1:
            case ByteCode.LSTORE_2:
            case ByteCode.LSTORE_3:
            case ByteCode.LSUB:
            case ByteCode.LXOR:
            case ByteCode.POP2:
                return -2;

            case ByteCode.AALOAD:
            case ByteCode.ARETURN:
            case ByteCode.ASTORE:
            case ByteCode.ASTORE_0:
            case ByteCode.ASTORE_1:
            case ByteCode.ASTORE_2:
            case ByteCode.ASTORE_3:
            case ByteCode.ATHROW:
            case ByteCode.BALOAD:
            case ByteCode.CALOAD:
            case ByteCode.D2F:
            case ByteCode.D2I:
            case ByteCode.FADD:
            case ByteCode.FALOAD:
            case ByteCode.FCMPG:
            case ByteCode.FCMPL:
            case ByteCode.FDIV:
            case ByteCode.FMUL:
            case ByteCode.FREM:
            case ByteCode.FRETURN:
            case ByteCode.FSTORE:
            case ByteCode.FSTORE_0:
            case ByteCode.FSTORE_1:
            case ByteCode.FSTORE_2:
            case ByteCode.FSTORE_3:
            case ByteCode.FSUB:
            case ByteCode.GETFIELD:
            case ByteCode.IADD:
            case ByteCode.IALOAD:
            case ByteCode.IAND:
            case ByteCode.IDIV:
            case ByteCode.IFEQ:
            case ByteCode.IFGE:
            case ByteCode.IFGT:
            case ByteCode.IFLE:
            case ByteCode.IFLT:
            case ByteCode.IFNE:
            case ByteCode.IFNONNULL:
            case ByteCode.IFNULL:
            case ByteCode.IMUL:
            case ByteCode.INVOKEINTERFACE:       //
            case ByteCode.INVOKESPECIAL:         // but needs to account for
            case ByteCode.INVOKEVIRTUAL:         // pops 'this' (unless static)
            case ByteCode.IOR:
            case ByteCode.IREM:
            case ByteCode.IRETURN:
            case ByteCode.ISHL:
            case ByteCode.ISHR:
            case ByteCode.ISTORE:
            case ByteCode.ISTORE_0:
            case ByteCode.ISTORE_1:
            case ByteCode.ISTORE_2:
            case ByteCode.ISTORE_3:
            case ByteCode.ISUB:
            case ByteCode.IUSHR:
            case ByteCode.IXOR:
            case ByteCode.L2F:
            case ByteCode.L2I:
            case ByteCode.LOOKUPSWITCH:
            case ByteCode.LSHL:
            case ByteCode.LSHR:
            case ByteCode.LUSHR:
            case ByteCode.MONITORENTER:
            case ByteCode.MONITOREXIT:
            case ByteCode.POP:
            case ByteCode.PUTFIELD:
            case ByteCode.SALOAD:
            case ByteCode.TABLESWITCH:
                return -1;

            case ByteCode.ANEWARRAY:
            case ByteCode.ARRAYLENGTH:
            case ByteCode.BREAKPOINT:
            case ByteCode.CHECKCAST:
            case ByteCode.D2L:
            case ByteCode.DALOAD:
            case ByteCode.DNEG:
            case ByteCode.F2I:
            case ByteCode.FNEG:
            case ByteCode.GETSTATIC:
            case ByteCode.GOTO:
            case ByteCode.GOTO_W:
            case ByteCode.I2B:
            case ByteCode.I2C:
            case ByteCode.I2F:
            case ByteCode.I2S:
            case ByteCode.IINC:
            case ByteCode.IMPDEP1:
            case ByteCode.IMPDEP2:
            case ByteCode.INEG:
            case ByteCode.INSTANCEOF:
            case ByteCode.INVOKESTATIC:
            case ByteCode.L2D:
            case ByteCode.LALOAD:
            case ByteCode.LNEG:
            case ByteCode.NEWARRAY:
            case ByteCode.NOP:
            case ByteCode.PUTSTATIC:
            case ByteCode.RET:
            case ByteCode.RETURN:
            case ByteCode.SWAP:
            case ByteCode.WIDE:
                return 0;

            case ByteCode.ACONST_NULL:
            case ByteCode.ALOAD:
            case ByteCode.ALOAD_0:
            case ByteCode.ALOAD_1:
            case ByteCode.ALOAD_2:
            case ByteCode.ALOAD_3:
            case ByteCode.BIPUSH:
            case ByteCode.DUP:
            case ByteCode.DUP_X1:
            case ByteCode.DUP_X2:
            case ByteCode.F2D:
            case ByteCode.F2L:
            case ByteCode.FCONST_0:
            case ByteCode.FCONST_1:
            case ByteCode.FCONST_2:
            case ByteCode.FLOAD:
            case ByteCode.FLOAD_0:
            case ByteCode.FLOAD_1:
            case ByteCode.FLOAD_2:
            case ByteCode.FLOAD_3:
            case ByteCode.I2D:
            case ByteCode.I2L:
            case ByteCode.ICONST_0:
            case ByteCode.ICONST_1:
            case ByteCode.ICONST_2:
            case ByteCode.ICONST_3:
            case ByteCode.ICONST_4:
            case ByteCode.ICONST_5:
            case ByteCode.ICONST_M1:
            case ByteCode.ILOAD:
            case ByteCode.ILOAD_0:
            case ByteCode.ILOAD_1:
            case ByteCode.ILOAD_2:
            case ByteCode.ILOAD_3:
            case ByteCode.JSR:
            case ByteCode.JSR_W:
            case ByteCode.LDC:
            case ByteCode.LDC_W:
            case ByteCode.MULTIANEWARRAY:
            case ByteCode.NEW:
            case ByteCode.SIPUSH:
                return 1;

            case ByteCode.DCONST_0:
            case ByteCode.DCONST_1:
            case ByteCode.DLOAD:
            case ByteCode.DLOAD_0:
            case ByteCode.DLOAD_1:
            case ByteCode.DLOAD_2:
            case ByteCode.DLOAD_3:
            case ByteCode.DUP2:
            case ByteCode.DUP2_X1:
            case ByteCode.DUP2_X2:
            case ByteCode.LCONST_0:
            case ByteCode.LCONST_1:
            case ByteCode.LDC2_W:
            case ByteCode.LLOAD:
            case ByteCode.LLOAD_0:
            case ByteCode.LLOAD_1:
            case ByteCode.LLOAD_2:
            case ByteCode.LLOAD_3:
                return 2;
        }
        throw new IllegalArgumentException("Bad opcode: "+opcode);
    }

        /*
         * Number of bytes of operands generated after the opcode.
         * Not in use currently.
         */
/*
    int extra(int opcode)
    {
        switch (opcode) {
            case ByteCode.AALOAD:
            case ByteCode.AASTORE:
            case ByteCode.ACONST_NULL:
            case ByteCode.ALOAD_0:
            case ByteCode.ALOAD_1:
            case ByteCode.ALOAD_2:
            case ByteCode.ALOAD_3:
            case ByteCode.ARETURN:
            case ByteCode.ARRAYLENGTH:
            case ByteCode.ASTORE_0:
            case ByteCode.ASTORE_1:
            case ByteCode.ASTORE_2:
            case ByteCode.ASTORE_3:
            case ByteCode.ATHROW:
            case ByteCode.BALOAD:
            case ByteCode.BASTORE:
            case ByteCode.BREAKPOINT:
            case ByteCode.CALOAD:
            case ByteCode.CASTORE:
            case ByteCode.D2F:
            case ByteCode.D2I:
            case ByteCode.D2L:
            case ByteCode.DADD:
            case ByteCode.DALOAD:
            case ByteCode.DASTORE:
            case ByteCode.DCMPG:
            case ByteCode.DCMPL:
            case ByteCode.DCONST_0:
            case ByteCode.DCONST_1:
            case ByteCode.DDIV:
            case ByteCode.DLOAD_0:
            case ByteCode.DLOAD_1:
            case ByteCode.DLOAD_2:
            case ByteCode.DLOAD_3:
            case ByteCode.DMUL:
            case ByteCode.DNEG:
            case ByteCode.DREM:
            case ByteCode.DRETURN:
            case ByteCode.DSTORE_0:
            case ByteCode.DSTORE_1:
            case ByteCode.DSTORE_2:
            case ByteCode.DSTORE_3:
            case ByteCode.DSUB:
            case ByteCode.DUP2:
            case ByteCode.DUP2_X1:
            case ByteCode.DUP2_X2:
            case ByteCode.DUP:
            case ByteCode.DUP_X1:
            case ByteCode.DUP_X2:
            case ByteCode.F2D:
            case ByteCode.F2I:
            case ByteCode.F2L:
            case ByteCode.FADD:
            case ByteCode.FALOAD:
            case ByteCode.FASTORE:
            case ByteCode.FCMPG:
            case ByteCode.FCMPL:
            case ByteCode.FCONST_0:
            case ByteCode.FCONST_1:
            case ByteCode.FCONST_2:
            case ByteCode.FDIV:
            case ByteCode.FLOAD_0:
            case ByteCode.FLOAD_1:
            case ByteCode.FLOAD_2:
            case ByteCode.FLOAD_3:
            case ByteCode.FMUL:
            case ByteCode.FNEG:
            case ByteCode.FREM:
            case ByteCode.FRETURN:
            case ByteCode.FSTORE_0:
            case ByteCode.FSTORE_1:
            case ByteCode.FSTORE_2:
            case ByteCode.FSTORE_3:
            case ByteCode.FSUB:
            case ByteCode.I2B:
            case ByteCode.I2C:
            case ByteCode.I2D:
            case ByteCode.I2F:
            case ByteCode.I2L:
            case ByteCode.I2S:
            case ByteCode.IADD:
            case ByteCode.IALOAD:
            case ByteCode.IAND:
            case ByteCode.IASTORE:
            case ByteCode.ICONST_0:
            case ByteCode.ICONST_1:
            case ByteCode.ICONST_2:
            case ByteCode.ICONST_3:
            case ByteCode.ICONST_4:
            case ByteCode.ICONST_5:
            case ByteCode.ICONST_M1:
            case ByteCode.IDIV:
            case ByteCode.ILOAD_0:
            case ByteCode.ILOAD_1:
            case ByteCode.ILOAD_2:
            case ByteCode.ILOAD_3:
            case ByteCode.IMPDEP1:
            case ByteCode.IMPDEP2:
            case ByteCode.IMUL:
            case ByteCode.INEG:
            case ByteCode.IOR:
            case ByteCode.IREM:
            case ByteCode.IRETURN:
            case ByteCode.ISHL:
            case ByteCode.ISHR:
            case ByteCode.ISTORE_0:
            case ByteCode.ISTORE_1:
            case ByteCode.ISTORE_2:
            case ByteCode.ISTORE_3:
            case ByteCode.ISUB:
            case ByteCode.IUSHR:
            case ByteCode.IXOR:
            case ByteCode.L2D:
            case ByteCode.L2F:
            case ByteCode.L2I:
            case ByteCode.LADD:
            case ByteCode.LALOAD:
            case ByteCode.LAND:
            case ByteCode.LASTORE:
            case ByteCode.LCMP:
            case ByteCode.LCONST_0:
            case ByteCode.LCONST_1:
            case ByteCode.LDIV:
            case ByteCode.LLOAD_0:
            case ByteCode.LLOAD_1:
            case ByteCode.LLOAD_2:
            case ByteCode.LLOAD_3:
            case ByteCode.LMUL:
            case ByteCode.LNEG:
            case ByteCode.LOR:
            case ByteCode.LREM:
            case ByteCode.LRETURN:
            case ByteCode.LSHL:
            case ByteCode.LSHR:
            case ByteCode.LSTORE_0:
            case ByteCode.LSTORE_1:
            case ByteCode.LSTORE_2:
            case ByteCode.LSTORE_3:
            case ByteCode.LSUB:
            case ByteCode.LUSHR:
            case ByteCode.LXOR:
            case ByteCode.MONITORENTER:
            case ByteCode.MONITOREXIT:
            case ByteCode.NOP:
            case ByteCode.POP2:
            case ByteCode.POP:
            case ByteCode.RETURN:
            case ByteCode.SALOAD:
            case ByteCode.SASTORE:
            case ByteCode.SWAP:
            case ByteCode.WIDE:
                return 0;

            case ByteCode.ALOAD:
            case ByteCode.ASTORE:
            case ByteCode.BIPUSH:
            case ByteCode.DLOAD:
            case ByteCode.DSTORE:
            case ByteCode.FLOAD:
            case ByteCode.FSTORE:
            case ByteCode.ILOAD:
            case ByteCode.ISTORE:
            case ByteCode.LDC:
            case ByteCode.LLOAD:
            case ByteCode.LSTORE:
            case ByteCode.NEWARRAY:
            case ByteCode.RET:
                return 1;

            case ByteCode.ANEWARRAY:
            case ByteCode.CHECKCAST:
            case ByteCode.GETFIELD:
            case ByteCode.GETSTATIC:
            case ByteCode.GOTO:
            case ByteCode.IFEQ:
            case ByteCode.IFGE:
            case ByteCode.IFGT:
            case ByteCode.IFLE:
            case ByteCode.IFLT:
            case ByteCode.IFNE:
            case ByteCode.IFNONNULL:
            case ByteCode.IFNULL:
            case ByteCode.IF_ACMPEQ:
            case ByteCode.IF_ACMPNE:
            case ByteCode.IF_ICMPEQ:
            case ByteCode.IF_ICMPGE:
            case ByteCode.IF_ICMPGT:
            case ByteCode.IF_ICMPLE:
            case ByteCode.IF_ICMPLT:
            case ByteCode.IF_ICMPNE:
            case ByteCode.IINC:
            case ByteCode.INSTANCEOF:
            case ByteCode.INVOKEINTERFACE:
            case ByteCode.INVOKESPECIAL:
            case ByteCode.INVOKESTATIC:
            case ByteCode.INVOKEVIRTUAL:
            case ByteCode.JSR:
            case ByteCode.LDC2_W:
            case ByteCode.LDC_W:
            case ByteCode.NEW:
            case ByteCode.PUTFIELD:
            case ByteCode.PUTSTATIC:
            case ByteCode.SIPUSH:
                return 2;

            case ByteCode.MULTIANEWARRAY:
                return 3;

            case ByteCode.GOTO_W:
            case ByteCode.JSR_W:
                return 4;

            case ByteCode.LOOKUPSWITCH:    // depends on alignment
            case ByteCode.TABLESWITCH: // depends on alignment
                return -1;
        }
        throw new IllegalArgumentException("Bad opcode: "+opcode);
    }
*/
    private static String bytecodeStr(int code)
    {
        if (DEBUGSTACK || DEBUGCODE) {
            switch (code) {
                case ByteCode.NOP:              return "nop";
                case ByteCode.ACONST_NULL:      return "aconst_null";
                case ByteCode.ICONST_M1:        return "iconst_m1";
                case ByteCode.ICONST_0:         return "iconst_0";
                case ByteCode.ICONST_1:         return "iconst_1";
                case ByteCode.ICONST_2:         return "iconst_2";
                case ByteCode.ICONST_3:         return "iconst_3";
                case ByteCode.ICONST_4:         return "iconst_4";
                case ByteCode.ICONST_5:         return "iconst_5";
                case ByteCode.LCONST_0:         return "lconst_0";
                case ByteCode.LCONST_1:         return "lconst_1";
                case ByteCode.FCONST_0:         return "fconst_0";
                case ByteCode.FCONST_1:         return "fconst_1";
                case ByteCode.FCONST_2:         return "fconst_2";
                case ByteCode.DCONST_0:         return "dconst_0";
                case ByteCode.DCONST_1:         return "dconst_1";
                case ByteCode.BIPUSH:           return "bipush";
                case ByteCode.SIPUSH:           return "sipush";
                case ByteCode.LDC:              return "ldc";
                case ByteCode.LDC_W:            return "ldc_w";
                case ByteCode.LDC2_W:           return "ldc2_w";
                case ByteCode.ILOAD:            return "iload";
                case ByteCode.LLOAD:            return "lload";
                case ByteCode.FLOAD:            return "fload";
                case ByteCode.DLOAD:            return "dload";
                case ByteCode.ALOAD:            return "aload";
                case ByteCode.ILOAD_0:          return "iload_0";
                case ByteCode.ILOAD_1:          return "iload_1";
                case ByteCode.ILOAD_2:          return "iload_2";
                case ByteCode.ILOAD_3:          return "iload_3";
                case ByteCode.LLOAD_0:          return "lload_0";
                case ByteCode.LLOAD_1:          return "lload_1";
                case ByteCode.LLOAD_2:          return "lload_2";
                case ByteCode.LLOAD_3:          return "lload_3";
                case ByteCode.FLOAD_0:          return "fload_0";
                case ByteCode.FLOAD_1:          return "fload_1";
                case ByteCode.FLOAD_2:          return "fload_2";
                case ByteCode.FLOAD_3:          return "fload_3";
                case ByteCode.DLOAD_0:          return "dload_0";
                case ByteCode.DLOAD_1:          return "dload_1";
                case ByteCode.DLOAD_2:          return "dload_2";
                case ByteCode.DLOAD_3:          return "dload_3";
                case ByteCode.ALOAD_0:          return "aload_0";
                case ByteCode.ALOAD_1:          return "aload_1";
                case ByteCode.ALOAD_2:          return "aload_2";
                case ByteCode.ALOAD_3:          return "aload_3";
                case ByteCode.IALOAD:           return "iaload";
                case ByteCode.LALOAD:           return "laload";
                case ByteCode.FALOAD:           return "faload";
                case ByteCode.DALOAD:           return "daload";
                case ByteCode.AALOAD:           return "aaload";
                case ByteCode.BALOAD:           return "baload";
                case ByteCode.CALOAD:           return "caload";
                case ByteCode.SALOAD:           return "saload";
                case ByteCode.ISTORE:           return "istore";
                case ByteCode.LSTORE:           return "lstore";
                case ByteCode.FSTORE:           return "fstore";
                case ByteCode.DSTORE:           return "dstore";
                case ByteCode.ASTORE:           return "astore";
                case ByteCode.ISTORE_0:         return "istore_0";
                case ByteCode.ISTORE_1:         return "istore_1";
                case ByteCode.ISTORE_2:         return "istore_2";
                case ByteCode.ISTORE_3:         return "istore_3";
                case ByteCode.LSTORE_0:         return "lstore_0";
                case ByteCode.LSTORE_1:         return "lstore_1";
                case ByteCode.LSTORE_2:         return "lstore_2";
                case ByteCode.LSTORE_3:         return "lstore_3";
                case ByteCode.FSTORE_0:         return "fstore_0";
                case ByteCode.FSTORE_1:         return "fstore_1";
                case ByteCode.FSTORE_2:         return "fstore_2";
                case ByteCode.FSTORE_3:         return "fstore_3";
                case ByteCode.DSTORE_0:         return "dstore_0";
                case ByteCode.DSTORE_1:         return "dstore_1";
                case ByteCode.DSTORE_2:         return "dstore_2";
                case ByteCode.DSTORE_3:         return "dstore_3";
                case ByteCode.ASTORE_0:         return "astore_0";
                case ByteCode.ASTORE_1:         return "astore_1";
                case ByteCode.ASTORE_2:         return "astore_2";
                case ByteCode.ASTORE_3:         return "astore_3";
                case ByteCode.IASTORE:          return "iastore";
                case ByteCode.LASTORE:          return "lastore";
                case ByteCode.FASTORE:          return "fastore";
                case ByteCode.DASTORE:          return "dastore";
                case ByteCode.AASTORE:          return "aastore";
                case ByteCode.BASTORE:          return "bastore";
                case ByteCode.CASTORE:          return "castore";
                case ByteCode.SASTORE:          return "sastore";
                case ByteCode.POP:              return "pop";
                case ByteCode.POP2:             return "pop2";
                case ByteCode.DUP:              return "dup";
                case ByteCode.DUP_X1:           return "dup_x1";
                case ByteCode.DUP_X2:           return "dup_x2";
                case ByteCode.DUP2:             return "dup2";
                case ByteCode.DUP2_X1:          return "dup2_x1";
                case ByteCode.DUP2_X2:          return "dup2_x2";
                case ByteCode.SWAP:             return "swap";
                case ByteCode.IADD:             return "iadd";
                case ByteCode.LADD:             return "ladd";
                case ByteCode.FADD:             return "fadd";
                case ByteCode.DADD:             return "dadd";
                case ByteCode.ISUB:             return "isub";
                case ByteCode.LSUB:             return "lsub";
                case ByteCode.FSUB:             return "fsub";
                case ByteCode.DSUB:             return "dsub";
                case ByteCode.IMUL:             return "imul";
                case ByteCode.LMUL:             return "lmul";
                case ByteCode.FMUL:             return "fmul";
                case ByteCode.DMUL:             return "dmul";
                case ByteCode.IDIV:             return "idiv";
                case ByteCode.LDIV:             return "ldiv";
                case ByteCode.FDIV:             return "fdiv";
                case ByteCode.DDIV:             return "ddiv";
                case ByteCode.IREM:             return "irem";
                case ByteCode.LREM:             return "lrem";
                case ByteCode.FREM:             return "frem";
                case ByteCode.DREM:             return "drem";
                case ByteCode.INEG:             return "ineg";
                case ByteCode.LNEG:             return "lneg";
                case ByteCode.FNEG:             return "fneg";
                case ByteCode.DNEG:             return "dneg";
                case ByteCode.ISHL:             return "ishl";
                case ByteCode.LSHL:             return "lshl";
                case ByteCode.ISHR:             return "ishr";
                case ByteCode.LSHR:             return "lshr";
                case ByteCode.IUSHR:            return "iushr";
                case ByteCode.LUSHR:            return "lushr";
                case ByteCode.IAND:             return "iand";
                case ByteCode.LAND:             return "land";
                case ByteCode.IOR:              return "ior";
                case ByteCode.LOR:              return "lor";
                case ByteCode.IXOR:             return "ixor";
                case ByteCode.LXOR:             return "lxor";
                case ByteCode.IINC:             return "iinc";
                case ByteCode.I2L:              return "i2l";
                case ByteCode.I2F:              return "i2f";
                case ByteCode.I2D:              return "i2d";
                case ByteCode.L2I:              return "l2i";
                case ByteCode.L2F:              return "l2f";
                case ByteCode.L2D:              return "l2d";
                case ByteCode.F2I:              return "f2i";
                case ByteCode.F2L:              return "f2l";
                case ByteCode.F2D:              return "f2d";
                case ByteCode.D2I:              return "d2i";
                case ByteCode.D2L:              return "d2l";
                case ByteCode.D2F:              return "d2f";
                case ByteCode.I2B:              return "i2b";
                case ByteCode.I2C:              return "i2c";
                case ByteCode.I2S:              return "i2s";
                case ByteCode.LCMP:             return "lcmp";
                case ByteCode.FCMPL:            return "fcmpl";
                case ByteCode.FCMPG:            return "fcmpg";
                case ByteCode.DCMPL:            return "dcmpl";
                case ByteCode.DCMPG:            return "dcmpg";
                case ByteCode.IFEQ:             return "ifeq";
                case ByteCode.IFNE:             return "ifne";
                case ByteCode.IFLT:             return "iflt";
                case ByteCode.IFGE:             return "ifge";
                case ByteCode.IFGT:             return "ifgt";
                case ByteCode.IFLE:             return "ifle";
                case ByteCode.IF_ICMPEQ:        return "if_icmpeq";
                case ByteCode.IF_ICMPNE:        return "if_icmpne";
                case ByteCode.IF_ICMPLT:        return "if_icmplt";
                case ByteCode.IF_ICMPGE:        return "if_icmpge";
                case ByteCode.IF_ICMPGT:        return "if_icmpgt";
                case ByteCode.IF_ICMPLE:        return "if_icmple";
                case ByteCode.IF_ACMPEQ:        return "if_acmpeq";
                case ByteCode.IF_ACMPNE:        return "if_acmpne";
                case ByteCode.GOTO:             return "goto";
                case ByteCode.JSR:              return "jsr";
                case ByteCode.RET:              return "ret";
                case ByteCode.TABLESWITCH:      return "tableswitch";
                case ByteCode.LOOKUPSWITCH:     return "lookupswitch";
                case ByteCode.IRETURN:          return "ireturn";
                case ByteCode.LRETURN:          return "lreturn";
                case ByteCode.FRETURN:          return "freturn";
                case ByteCode.DRETURN:          return "dreturn";
                case ByteCode.ARETURN:          return "areturn";
                case ByteCode.RETURN:           return "return";
                case ByteCode.GETSTATIC:        return "getstatic";
                case ByteCode.PUTSTATIC:        return "putstatic";
                case ByteCode.GETFIELD:         return "getfield";
                case ByteCode.PUTFIELD:         return "putfield";
                case ByteCode.INVOKEVIRTUAL:    return "invokevirtual";
                case ByteCode.INVOKESPECIAL:    return "invokespecial";
                case ByteCode.INVOKESTATIC:     return "invokestatic";
                case ByteCode.INVOKEINTERFACE:  return "invokeinterface";
                case ByteCode.NEW:              return "new";
                case ByteCode.NEWARRAY:         return "newarray";
                case ByteCode.ANEWARRAY:        return "anewarray";
                case ByteCode.ARRAYLENGTH:      return "arraylength";
                case ByteCode.ATHROW:           return "athrow";
                case ByteCode.CHECKCAST:        return "checkcast";
                case ByteCode.INSTANCEOF:       return "instanceof";
                case ByteCode.MONITORENTER:     return "monitorenter";
                case ByteCode.MONITOREXIT:      return "monitorexit";
                case ByteCode.WIDE:             return "wide";
                case ByteCode.MULTIANEWARRAY:   return "multianewarray";
                case ByteCode.IFNULL:           return "ifnull";
                case ByteCode.IFNONNULL:        return "ifnonnull";
                case ByteCode.GOTO_W:           return "goto_w";
                case ByteCode.JSR_W:            return "jsr_w";
                case ByteCode.BREAKPOINT:       return "breakpoint";

                case ByteCode.IMPDEP1:          return "impdep1";
                case ByteCode.IMPDEP2:          return "impdep2";
            }
        }
        return "";
    }

    final char[] getCharBuffer(int minimalSize)
    {
        if (minimalSize > tmpCharBuffer.length) {
            int newSize = tmpCharBuffer.length * 2;
            if (minimalSize > newSize) { newSize = minimalSize; }
            tmpCharBuffer = new char[newSize];
        }
        return tmpCharBuffer;
    }

    private static final int LineNumberTableSize = 16;
    private static final int ExceptionTableSize = 4;

    private final static long FileHeaderConstant = 0xCAFEBABE0003002DL;
    // Set DEBUG flags to true to get better checking and progress info.
    private static final boolean DEBUGSTACK = false;
    private static final boolean DEBUGLABELS = false;
    private static final boolean DEBUGCODE = false;
    private static final int CodeBufferSize = 128;

    private String generatedClassName;

    private ExceptionTableEntry itsExceptionTable[];
    private int itsExceptionTableTop;

    private int itsLineNumberTable[];   // pack start_pc & line_number together
    private int itsLineNumberTableTop;

    private byte[] itsCodeBuffer = new byte[256];
    private int itsCodeBufferTop;

    private ConstantPool itsConstantPool;

    private short itsSourceFileAttributeIndex;

    private ClassFileMethod itsCurrentMethod;
    private short itsStackTop;

    private short itsMaxStack;
    private short itsMaxLocals;

    private ObjArray itsMethods = new ObjArray();
    private ObjArray itsFields = new ObjArray();
    private ObjArray itsInterfaces = new ObjArray();

    private short itsFlags;
    private short itsThisClassIndex;
    private short itsSuperClassIndex;
    private short itsSourceFileNameIndex;

    private static final int MIN_LABEL_TABLE_SIZE = 32;
    private int[] itsLabelTable;
    private int itsLabelTableTop;

// itsFixupTable[i] = (label_index << 32) | fixup_site
    private static final int MIN_FIXUP_TABLE_SIZE = 40;
    private long[] itsFixupTable;
    private int itsFixupTableTop;
    private ObjArray itsVarDescriptors;

    private char[] tmpCharBuffer = new char[64];
}

final class ExceptionTableEntry
{

    ExceptionTableEntry(int startLabel, int endLabel,
                        int handlerLabel, short catchType)
    {
        itsStartLabel = startLabel;
        itsEndLabel = endLabel;
        itsHandlerLabel = handlerLabel;
        itsCatchType = catchType;
    }

    int itsStartLabel;
    int itsEndLabel;
    int itsHandlerLabel;
    short itsCatchType;
}

final class ClassFileField
{

    ClassFileField(short nameIndex, short typeIndex, short flags)
    {
        itsNameIndex = nameIndex;
        itsTypeIndex = typeIndex;
        itsFlags = flags;
        itsHasAttributes = false;
    }

    void setAttributes(short attr1, short attr2, short attr3, int index)
    {
        itsHasAttributes = true;
        itsAttr1 = attr1;
        itsAttr2 = attr2;
        itsAttr3 = attr3;
        itsIndex = index;
    }

    int write(byte[] data, int offset)
    {
        offset = ClassFileWriter.putInt16(itsFlags, data, offset);
        offset = ClassFileWriter.putInt16(itsNameIndex, data, offset);
        offset = ClassFileWriter.putInt16(itsTypeIndex, data, offset);
        if (!itsHasAttributes) {
            // write 0 attributes
            offset = ClassFileWriter.putInt16(0, data, offset);
        } else {
            offset = ClassFileWriter.putInt16(1, data, offset);
            offset = ClassFileWriter.putInt16(itsAttr1, data, offset);
            offset = ClassFileWriter.putInt16(itsAttr2, data, offset);
            offset = ClassFileWriter.putInt16(itsAttr3, data, offset);
            offset = ClassFileWriter.putInt16(itsIndex, data, offset);
        }
        return offset;
    }

    int getWriteSize()
    {
        int size = 2 * 3;
        if (!itsHasAttributes) {
            size += 2;
        } else {
            size += 2 + 2 * 4;
        }
        return size;
    }

    private short itsNameIndex;
    private short itsTypeIndex;
    private short itsFlags;
    private boolean itsHasAttributes;
    private short itsAttr1, itsAttr2, itsAttr3;
    private int itsIndex;
}

final class ClassFileMethod
{

    ClassFileMethod(short nameIndex, short typeIndex, short flags)
    {
        itsNameIndex = nameIndex;
        itsTypeIndex = typeIndex;
        itsFlags = flags;
    }

    void setCodeAttribute(byte codeAttribute[])
    {
        itsCodeAttribute = codeAttribute;
    }

    int write(byte[] data, int offset)
    {
        offset = ClassFileWriter.putInt16(itsFlags, data, offset);
        offset = ClassFileWriter.putInt16(itsNameIndex, data, offset);
        offset = ClassFileWriter.putInt16(itsTypeIndex, data, offset);
        // Code attribute only
        offset = ClassFileWriter.putInt16(1, data, offset);
        System.arraycopy(itsCodeAttribute, 0, data, offset,
                         itsCodeAttribute.length);
        offset += itsCodeAttribute.length;
        return offset;
    }

    int getWriteSize()
    {
        return 2 * 4 + itsCodeAttribute.length;
    }

    private short itsNameIndex;
    private short itsTypeIndex;
    private short itsFlags;
    private byte[] itsCodeAttribute;

}

final class ConstantPool
{

    ConstantPool(ClassFileWriter cfw)
    {
        this.cfw = cfw;
        itsTopIndex = 1;       // the zero'th entry is reserved
        itsPool = new byte[ConstantPoolSize];
        itsTop = 0;
    }

    private static final int ConstantPoolSize = 256;
    private static final byte
        CONSTANT_Class = 7,
        CONSTANT_Fieldref = 9,
        CONSTANT_Methodref = 10,
        CONSTANT_InterfaceMethodref = 11,
        CONSTANT_String = 8,
        CONSTANT_Integer = 3,
        CONSTANT_Float = 4,
        CONSTANT_Long = 5,
        CONSTANT_Double = 6,
        CONSTANT_NameAndType = 12,
        CONSTANT_Utf8 = 1;

    int write(byte[] data, int offset)
    {
        offset = ClassFileWriter.putInt16((short)itsTopIndex, data, offset);
        System.arraycopy(itsPool, 0, data, offset, itsTop);
        offset += itsTop;
        return offset;
    }

    int getWriteSize()
    {
        return 2 + itsTop;
    }

    int addConstant(int k)
    {
        ensure(5);
        itsPool[itsTop++] = CONSTANT_Integer;
        itsTop = ClassFileWriter.putInt32(k, itsPool, itsTop);
        return (short)(itsTopIndex++);
    }

    int addConstant(long k)
    {
        ensure(9);
        itsPool[itsTop++] = CONSTANT_Long;
        itsTop = ClassFileWriter.putInt64(k, itsPool, itsTop);
        int index = itsTopIndex;
        itsTopIndex += 2;
        return index;
    }

    int addConstant(float k)
    {
        ensure(5);
        itsPool[itsTop++] = CONSTANT_Float;
        int bits = Float.floatToIntBits(k);
        itsTop = ClassFileWriter.putInt32(bits, itsPool, itsTop);
        return itsTopIndex++;
    }

    int addConstant(double k)
    {
        ensure(9);
        itsPool[itsTop++] = CONSTANT_Double;
        long bits = Double.doubleToLongBits(k);
        itsTop = ClassFileWriter.putInt64(bits, itsPool, itsTop);
        int index = itsTopIndex;
        itsTopIndex += 2;
        return index;
    }

    int addConstant(String k)
    {
        int utf8Index = 0xFFFF & addUtf8(k);
        int theIndex = itsStringConstHash.getInt(utf8Index, -1);
        if (theIndex == -1) {
            theIndex = itsTopIndex++;
            ensure(3);
            itsPool[itsTop++] = CONSTANT_String;
            itsTop = ClassFileWriter.putInt16(utf8Index, itsPool, itsTop);
            itsStringConstHash.put(utf8Index, theIndex);
        }
        return theIndex;
    }

    boolean isUnderUtfEncodingLimit(String s)
    {
        int strLen = s.length();
        if (strLen * 3 <= MAX_UTF_ENCODING_SIZE) {
            return true;
        } else if (strLen > MAX_UTF_ENCODING_SIZE) {
            return false;
        }
        return strLen == getUtfEncodingLimit(s, 0, strLen);
    }

    /**
     * Get maximum i such that <tt>start <= i <= end</tt> and
     * <tt>s.substring(start, i)</tt> fits JVM UTF string encoding limit.
     */
    int getUtfEncodingLimit(String s, int start, int end)
    {
        if ((end - start) * 3 <= MAX_UTF_ENCODING_SIZE) {
            return end;
        }
        int limit = MAX_UTF_ENCODING_SIZE;
        for (int i = start; i != end; i++) {
            int c = s.charAt(i);
            if (0 != c && c <= 0x7F) {
                --limit;
            } else if (c < 0x7FF) {
                limit -= 2;
            } else {
                limit -= 3;
            }
            if (limit < 0) {
                return i;
            }
        }
        return end;
    }

    short addUtf8(String k)
    {
        int theIndex = itsUtf8Hash.get(k, -1);
        if (theIndex == -1) {
            int strLen = k.length();
            boolean tooBigString;
            if (strLen > MAX_UTF_ENCODING_SIZE) {
                tooBigString = true;
            } else {
                tooBigString = false;
                // Ask for worst case scenario buffer when each char takes 3
                // bytes
                ensure(1 + 2 + strLen * 3);
                int top = itsTop;

                itsPool[top++] = CONSTANT_Utf8;
                top += 2; // skip length

                char[] chars = cfw.getCharBuffer(strLen);
                k.getChars(0, strLen, chars, 0);

                for (int i = 0; i != strLen; i++) {
                    int c = chars[i];
                    if (c != 0 && c <= 0x7F) {
                        itsPool[top++] = (byte)c;
                    } else if (c > 0x7FF) {
                        itsPool[top++] = (byte)(0xE0 | (c >> 12));
                        itsPool[top++] = (byte)(0x80 | ((c >> 6) & 0x3F));
                        itsPool[top++] = (byte)(0x80 | (c & 0x3F));
                    } else {
                        itsPool[top++] = (byte)(0xC0 | (c >> 6));
                        itsPool[top++] = (byte)(0x80 | (c & 0x3F));
                    }
                }

                int utfLen = top - (itsTop + 1 + 2);
                if (utfLen > MAX_UTF_ENCODING_SIZE) {
                    tooBigString = true;
                } else {
                    // Write back length
                    itsPool[itsTop + 1] = (byte)(utfLen >>> 8);
                    itsPool[itsTop + 2] = (byte)utfLen;

                    itsTop = top;
                    theIndex = itsTopIndex++;
                    itsUtf8Hash.put(k, theIndex);
                }
            }
            if (tooBigString) {
                throw new IllegalArgumentException("Too big string");
            }
        }
        return (short)theIndex;
    }

    private short addNameAndType(String name, String type)
    {
        short nameIndex = addUtf8(name);
        short typeIndex = addUtf8(type);
        ensure(5);
        itsPool[itsTop++] = CONSTANT_NameAndType;
        itsTop = ClassFileWriter.putInt16(nameIndex, itsPool, itsTop);
        itsTop = ClassFileWriter.putInt16(typeIndex, itsPool, itsTop);
        return (short)(itsTopIndex++);
    }

    short addClass(String className)
    {
        int theIndex = itsClassHash.get(className, -1);
        if (theIndex == -1) {
            String slashed = className;
            if (className.indexOf('.') > 0) {
                slashed = ClassFileWriter.getSlashedForm(className);
                theIndex = itsClassHash.get(slashed, -1);
                if (theIndex != -1) {
                    itsClassHash.put(className, theIndex);
                }
            }
            if (theIndex == -1) {
                int utf8Index = addUtf8(slashed);
                ensure(3);
                itsPool[itsTop++] = CONSTANT_Class;
                itsTop = ClassFileWriter.putInt16(utf8Index, itsPool, itsTop);
                theIndex = itsTopIndex++;
                itsClassHash.put(slashed, theIndex);
                if (className != slashed) {
                    itsClassHash.put(className, theIndex);
                }
            }
        }
        return (short)theIndex;
    }

    short addFieldRef(String className, String fieldName, String fieldType)
    {
        FieldOrMethodRef ref = new FieldOrMethodRef(className, fieldName,
                                                    fieldType);

        int theIndex = itsFieldRefHash.get(ref, -1);
        if (theIndex == -1) {
            short ntIndex = addNameAndType(fieldName, fieldType);
            short classIndex = addClass(className);
            ensure(5);
            itsPool[itsTop++] = CONSTANT_Fieldref;
            itsTop = ClassFileWriter.putInt16(classIndex, itsPool, itsTop);
            itsTop = ClassFileWriter.putInt16(ntIndex, itsPool, itsTop);
            theIndex = itsTopIndex++;
            itsFieldRefHash.put(ref, theIndex);
        }
        return (short)theIndex;
    }

    short addMethodRef(String className, String methodName,
                       String methodType)
    {
        FieldOrMethodRef ref = new FieldOrMethodRef(className, methodName,
                                                    methodType);

        int theIndex = itsMethodRefHash.get(ref, -1);
        if (theIndex == -1) {
            short ntIndex = addNameAndType(methodName, methodType);
            short classIndex = addClass(className);
            ensure(5);
            itsPool[itsTop++] = CONSTANT_Methodref;
            itsTop = ClassFileWriter.putInt16(classIndex, itsPool, itsTop);
            itsTop = ClassFileWriter.putInt16(ntIndex, itsPool, itsTop);
            theIndex = itsTopIndex++;
            itsMethodRefHash.put(ref, theIndex);
        }
        return (short)theIndex;
    }

    short addInterfaceMethodRef(String className,
                                String methodName, String methodType)
    {
        short ntIndex = addNameAndType(methodName, methodType);
        short classIndex = addClass(className);
        ensure(5);
        itsPool[itsTop++] = CONSTANT_InterfaceMethodref;
        itsTop = ClassFileWriter.putInt16(classIndex, itsPool, itsTop);
        itsTop = ClassFileWriter.putInt16(ntIndex, itsPool, itsTop);
        return (short)(itsTopIndex++);
    }

    void ensure(int howMuch)
    {
        if (itsTop + howMuch > itsPool.length) {
            int newCapacity = itsPool.length * 2;
            if (itsTop + howMuch > newCapacity) {
                newCapacity = itsTop + howMuch;
            }
            byte[] tmp = new byte[newCapacity];
            System.arraycopy(itsPool, 0, tmp, 0, itsTop);
            itsPool = tmp;
        }
    }

    private ClassFileWriter cfw;

    private static final int MAX_UTF_ENCODING_SIZE = 65535;

    private UintMap itsStringConstHash = new UintMap();
    private ObjToIntMap itsUtf8Hash = new ObjToIntMap();
    private ObjToIntMap itsFieldRefHash = new ObjToIntMap();
    private ObjToIntMap itsMethodRefHash = new ObjToIntMap();
    private UintMap nameAndTypeHash = new UintMap();
    private ObjToIntMap itsClassHash = new ObjToIntMap();

    private int itsTop;
    private int itsTopIndex;
    private byte itsPool[];
}

final class FieldOrMethodRef
{
    FieldOrMethodRef(String className, String name, String type)
    {
        this.className = className;
        this.name = name;
        this.type = type;
    }

    public boolean equals(Object obj)
    {
        if (!(obj instanceof FieldOrMethodRef)) { return false; }
        FieldOrMethodRef x = (FieldOrMethodRef)obj;
        return className.equals(x.className)
            && name.equals(x.name)
            && type.equals(x.type);
    }

    public int hashCode()
    {
        if (hashCode == -1) {
            int h1 = className.hashCode();
            int h2 = name.hashCode();
            int h3 = type.hashCode();
            hashCode = h1 ^ h2 ^ h3;
        }
        return hashCode;
    }

    private String className;
    private String name;
    private String type;
    private int hashCode = -1;
}
