/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.classfile;

import org.mozilla.javascript.*;

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
public class ClassFileWriter extends LabelTable {

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
        itsConstantPool = new ConstantPool();
        itsThisClassIndex = itsConstantPool.addClass(className);
        itsSuperClassIndex = itsConstantPool.addClass(superClassName);
        if (sourceFileName != null)
            itsSourceFileNameIndex = itsConstantPool.addUtf8(sourceFileName);
        itsFlags = ACC_PUBLIC;
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
        itsInterfaces.addElement(new Short(interfaceIndex));
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

    public static String fullyQualifiedForm(String name) {
    	return name.replace('.', '/');
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
        itsFields.addElement(
                new ClassFileField(fieldNameIndex, typeIndex, flags));
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
        short cvAttr[] = new short[4];
        cvAttr[0] = itsConstantPool.addUtf8("ConstantValue");
        cvAttr[1] = 0;
        cvAttr[2] = 2;
        cvAttr[3] = itsConstantPool.addConstant(value);
        itsFields.addElement(
                new ClassFileField(fieldNameIndex, typeIndex, flags, cvAttr));
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
        short cvAttr[] = new short[4];
        cvAttr[0] = itsConstantPool.addUtf8("ConstantValue");
        cvAttr[1] = 0;
        cvAttr[2] = 2;
        cvAttr[3] = itsConstantPool.addConstant(value);
        itsFields.addElement(
                new ClassFileField(fieldNameIndex, typeIndex, flags, cvAttr));
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
        short cvAttr[] = new short[4];
        cvAttr[0] = itsConstantPool.addUtf8("ConstantValue");
        cvAttr[1] = 0;
        cvAttr[2] = 2;
        cvAttr[3] = itsConstantPool.addConstant(value);
        itsFields.addElement(
                new ClassFileField(fieldNameIndex, typeIndex, flags, cvAttr));
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
        itsMethods.addElement(itsCurrentMethod);
    }
    
    /**
     * Complete generation of the method.
     * 
     * After this method is called, no more code can be added to the 
     * method begun with <code>startMethod</code>.
     * 
     * @param maxLocals the maximum number of local variable slots
     *        (a.k.a. Java registers) used by the method
     * @param vars the VariableTable of the variables for the method,
     *        or null if none
     */
    public void stopMethod(short maxLocals, VariableTable vars) {
        if (DEBUG) {
            if (itsCurrentMethod == null)
                throw new RuntimeException("No method to stop");
        }

        for (int i = 0; i < itsLabelTableTop; i++)
            itsLabelTable[i].fixGotos(itsCodeBuffer);

        itsMaxLocals = maxLocals;

        int lineNumberTableLength = 0;
        if (itsLineNumberTable != null) {
            // 6 bytes for the attribute header
            // 2 bytes for the line number count
            // 4 bytes for each entry
            lineNumberTableLength = 6 + 2 + (itsLineNumberTableTop * 4);
        }
        
        int variableTableLength = 0;
        if (vars != null) {
            // 6 bytes for the attribute header
            // 2 bytes for the variable count
            // 10 bytes for each entry
            variableTableLength = 6 + 2 + (vars.size() * 10);
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

        byte codeAttribute[] = new byte[attrLength];
        int index = 0;
        int codeAttrIndex = itsConstantPool.addUtf8("Code");
        codeAttribute[index++] = (byte)(codeAttrIndex >> 8);
        codeAttribute[index++] = (byte)codeAttrIndex;
        attrLength -= 6;                 // discount the attribute header
        codeAttribute[index++] = (byte)(attrLength >> 24);
        codeAttribute[index++] = (byte)(attrLength >> 16);
        codeAttribute[index++] = (byte)(attrLength >> 8);
        codeAttribute[index++] = (byte)attrLength;
        codeAttribute[index++] = (byte)(itsMaxStack >> 8);
        codeAttribute[index++] = (byte)itsMaxStack;
        codeAttribute[index++] = (byte)(itsMaxLocals >> 8);
        codeAttribute[index++] = (byte)itsMaxLocals;
        codeAttribute[index++] = (byte)(itsCodeBufferTop >> 24);
        codeAttribute[index++] = (byte)(itsCodeBufferTop >> 16);
        codeAttribute[index++] = (byte)(itsCodeBufferTop >> 8);
        codeAttribute[index++] = (byte)itsCodeBufferTop;
        System.arraycopy(itsCodeBuffer, 0, codeAttribute, index, 
                         itsCodeBufferTop);
        index += itsCodeBufferTop;
        

        if (itsExceptionTableTop > 0) {
            codeAttribute[index++] = (byte)(itsExceptionTableTop >> 8);
            codeAttribute[index++] = (byte)(itsExceptionTableTop);            
            for (int i = 0; i < itsExceptionTableTop; i++) {
                short startPC = itsExceptionTable[i].getStartPC(itsLabelTable);
                codeAttribute[index++] = (byte)(startPC >> 8);
                codeAttribute[index++] = (byte)(startPC);
                short endPC = itsExceptionTable[i].getEndPC(itsLabelTable);
                codeAttribute[index++] = (byte)(endPC >> 8);
                codeAttribute[index++] = (byte)(endPC);
                short handlerPC = itsExceptionTable[i].getHandlerPC(itsLabelTable);
                codeAttribute[index++] = (byte)(handlerPC >> 8);
                codeAttribute[index++] = (byte)(handlerPC);
                short catchType = itsExceptionTable[i].getCatchType();
                codeAttribute[index++] = (byte)(catchType >> 8);
                codeAttribute[index++] = (byte)(catchType);
            }            
        }
        else {
            codeAttribute[index++] = (byte)(0);     // exception table length
            codeAttribute[index++] = (byte)(0);
        }
        
        int attributeCount = 0;
        if (itsLineNumberTable != null)
            attributeCount++;
        if (vars != null)
            attributeCount++;
        codeAttribute[index++] = (byte)(0);     // (hibyte) attribute count...
        codeAttribute[index++] = (byte)(attributeCount); // (lobyte) attribute count
        
        if (itsLineNumberTable != null) {
            int lineNumberTableAttrIndex
                    = itsConstantPool.addUtf8("LineNumberTable");
            codeAttribute[index++] = (byte)(lineNumberTableAttrIndex >> 8);
            codeAttribute[index++] = (byte)lineNumberTableAttrIndex;
            int tableAttrLength = 2 + (itsLineNumberTableTop * 4);
            codeAttribute[index++] = (byte)(tableAttrLength >> 24);
            codeAttribute[index++] = (byte)(tableAttrLength >> 16);
            codeAttribute[index++] = (byte)(tableAttrLength >> 8);
            codeAttribute[index++] = (byte)tableAttrLength;
            codeAttribute[index++] = (byte)(itsLineNumberTableTop >> 8);
            codeAttribute[index++] = (byte)itsLineNumberTableTop;
            for (int i = 0; i < itsLineNumberTableTop; i++) {
                codeAttribute[index++] = (byte)(itsLineNumberTable[i] >> 24);
                codeAttribute[index++] = (byte)(itsLineNumberTable[i] >> 16);
                codeAttribute[index++] = (byte)(itsLineNumberTable[i] >> 8);
                codeAttribute[index++] = (byte)itsLineNumberTable[i];
            }
        }
        
        if (vars != null) {
            int variableTableAttrIndex
                    = itsConstantPool.addUtf8("LocalVariableTable");
            codeAttribute[index++] = (byte)(variableTableAttrIndex >> 8);
            codeAttribute[index++] = (byte)variableTableAttrIndex;
            int varCount = vars.size();
            int tableAttrLength = 2 + (varCount * 10);
            codeAttribute[index++] = (byte)(tableAttrLength >> 24);
            codeAttribute[index++] = (byte)(tableAttrLength >> 16);
            codeAttribute[index++] = (byte)(tableAttrLength >> 8);
            codeAttribute[index++] = (byte)tableAttrLength;
            codeAttribute[index++] = (byte)(varCount >> 8);
            codeAttribute[index++] = (byte)varCount;
            for (int i = 0; i < varCount; i++) {
                LocalVariable lvar = vars.get(i);
                // start pc
                int startPc = lvar.getStartPC();
                codeAttribute[index++] = (byte)(startPc >> 8);
                codeAttribute[index++] = (byte)startPc;
                
                // length
                int length = itsCodeBufferTop - startPc;
                codeAttribute[index++] = (byte)(length >> 8);
                codeAttribute[index++] = (byte)length;
                
                // name index
                int nameIndex
                        = itsConstantPool.addUtf8(lvar.getName());
                codeAttribute[index++] = (byte)(nameIndex >> 8);
                codeAttribute[index++] = (byte)nameIndex;
                
                // descriptor index
                int descriptorIndex = itsConstantPool.addUtf8(
                                        lvar.isNumber() 
                                        ? "D"
                                        :  "Ljava/lang/Object;");
                codeAttribute[index++] = (byte)(descriptorIndex >> 8);
                codeAttribute[index++] = (byte)descriptorIndex;
                
                // index
                int jreg = lvar.getJRegister();
                codeAttribute[index++] = (byte)(jreg >> 8);
                codeAttribute[index++] = (byte)jreg;                
            }            
        }

        itsCurrentMethod.setCodeAttribute(codeAttribute);

        itsExceptionTable = null;
        itsExceptionTableTop = 0;
        itsLabelTableTop = 0;
        itsLineNumberTable = null;
        itsCodeBufferTop = 0;
        itsCurrentMethod = null;
        itsMaxStack = 0;
        itsStackTop = 0;
    }

    /**
     * Add the single-byte opcode to the current method.
     * 
     * @param theOpCode the opcode of the bytecode
     */
    public void add(byte theOpCode) {
        if (DEBUGCODE)
            System.out.println("Add " + Integer.toHexString(theOpCode & 0xFF));
        if (DEBUG) {
            if (ByteCode.opcodeCount[theOpCode & 0xFF] != 0)
                throw new RuntimeException("Expected operands");
        }
        addToCodeBuffer(theOpCode);
        itsStackTop += ByteCode.stackChange[theOpCode & 0xFF];
        if (DEBUGSTACK) {
            System.out.println("After " + Integer.toHexString(theOpCode & 0xFF) + " stack = " + itsStackTop);
        }
        if (DEBUG) {
            if (itsStackTop < 0)
                throw new RuntimeException("Stack underflow");
        }
        if (itsStackTop > itsMaxStack) itsMaxStack = itsStackTop;
    }

    /**
     * Add a single-operand opcode to the current method.
     * 
     * @param theOpCode the opcode of the bytecode
     * @param theOperand the operand of the bytecode
     */
    public void add(byte theOpCode, int theOperand) {
        if (DEBUGCODE)
            System.out.println("Add " + Integer.toHexString(theOpCode & 0xFF) + ", " + Integer.toHexString(theOperand) );
        itsStackTop += ByteCode.stackChange[theOpCode & 0xFF];
        if (DEBUGSTACK) {
            System.out.println("After " + Integer.toHexString(theOpCode & 0xFF) + " stack = " + itsStackTop);
        }
        if (DEBUG) {
            if (itsStackTop < 0)
                throw new RuntimeException("Stack underflow");
        }
        if (itsStackTop > itsMaxStack) itsMaxStack = itsStackTop;

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
                    if (DEBUG) {
                        if ((theOperand & 0x80000000) != 0x80000000) {
                            if ((theOperand < 0) || (theOperand > 65535))
                                throw new RuntimeException("Bad label for branch");
                        }
                    }
                    int branchPC = itsCodeBufferTop;
                    addToCodeBuffer(theOpCode);
                    if ((theOperand & 0x80000000) != 0x80000000) {
                            // hard displacement
                        int temp_Label = acquireLabel();
                        int theLabel = temp_Label & 0x7FFFFFFF;
                        itsLabelTable[theLabel].setPC(
                                            (short)(branchPC + theOperand));
                        addToCodeBuffer((byte)(theOperand >> 8));
                        addToCodeBuffer((byte)theOperand);
                    }
                    else {  // a label
                        int theLabel = theOperand & 0x7FFFFFFF;
                        int targetPC = itsLabelTable[theLabel].getPC();
                        if (DEBUGLABELS) {
                            System.out.println("Fixing branch to " + 
                                               theLabel + " at " + targetPC + 
                                               " from " + branchPC);
                        }
                        if (targetPC != -1) {
                            short offset = (short)(targetPC - branchPC);
                            addToCodeBuffer((byte)(offset >> 8));
                            addToCodeBuffer((byte)offset);
                        }
                        else {
                            itsLabelTable[theLabel].addFixup(branchPC + 1);
                            addToCodeBuffer((byte)0);
                            addToCodeBuffer((byte)0);
                        }
                    }
                }
                break;

            case ByteCode.BIPUSH :
                if (DEBUG) {
                    if ((theOperand < -128) || (theOperand > 127))
                        throw new RuntimeException("out of range byte");
                }
                addToCodeBuffer(theOpCode);
                addToCodeBuffer((byte)theOperand);
                break;

            case ByteCode.SIPUSH :
                if (DEBUG) {
                    if ((theOperand < -32768) || (theOperand > 32767))
                        throw new RuntimeException("out of range short");
                }
                addToCodeBuffer(theOpCode);
                addToCodeBuffer((byte)(theOperand >> 8));
                addToCodeBuffer((byte)theOperand);
                break;

            case ByteCode.NEWARRAY :
                if (DEBUG) {
                    if ((theOperand < 0) || (theOperand > 255))
                        throw new RuntimeException("out of range index");
                }
                addToCodeBuffer(theOpCode);
                addToCodeBuffer((byte)theOperand);
                break;

            case ByteCode.GETFIELD :
            case ByteCode.PUTFIELD :
                if (DEBUG) {
                    if ((theOperand < 0) || (theOperand > 65535))
                        throw new RuntimeException("out of range field");
                }
                addToCodeBuffer(theOpCode);
                addToCodeBuffer((byte)(theOperand >> 8));
                addToCodeBuffer((byte)theOperand);
                break;

            case ByteCode.LDC :
            case ByteCode.LDC_W :
            case ByteCode.LDC2_W :
                if (DEBUG) {
                    if ((theOperand < 0) || (theOperand > 65535))
                        throw new RuntimeException("out of range index");
                }
                if ((theOperand >= 256)
                            || (theOpCode == ByteCode.LDC_W)
                            || (theOpCode == ByteCode.LDC2_W)) {
                    if (theOpCode == ByteCode.LDC)
                        addToCodeBuffer(ByteCode.LDC_W);
                    else
                        addToCodeBuffer(theOpCode);
                    addToCodeBuffer((byte)(theOperand >> 8));
                    addToCodeBuffer((byte)theOperand);
                }
                else {
                    addToCodeBuffer(theOpCode);
                    addToCodeBuffer((byte)theOperand);
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
                if (DEBUG) {
                    if ((theOperand < 0) || (theOperand > 65535))
                        throw new RuntimeException("out of range variable");
                }
                if (theOperand >= 256) {
                    addToCodeBuffer(ByteCode.WIDE);
                    addToCodeBuffer(theOpCode);
                    addToCodeBuffer((byte)(theOperand >> 8));
                    addToCodeBuffer((byte)theOperand);
                }
                else {
                    addToCodeBuffer(theOpCode);
                    addToCodeBuffer((byte)theOperand);
                }
                break;

            default :
                throw new RuntimeException("Unexpected opcode for 1 operand");
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
    public void add(byte theOpCode, int theOperand1, int theOperand2) {
        if (DEBUGCODE)
            System.out.println("Add " + Integer.toHexString(theOpCode & 0xFF)
                                    + ", " + Integer.toHexString(theOperand1)
                                     + ", " + Integer.toHexString(theOperand2));
        itsStackTop += ByteCode.stackChange[theOpCode & 0xFF];
        if (DEBUGSTACK) {
            System.out.println("After " + Integer.toHexString(theOpCode & 0xFF) + " stack = " + itsStackTop);
        }
        if (DEBUG) {
            if (itsStackTop < 0)
                throw new RuntimeException("Stack underflow");
        }
        if (itsStackTop > itsMaxStack) itsMaxStack = itsStackTop;

        if (theOpCode == ByteCode.IINC) {
            if (DEBUG) {
                if ((theOperand1 < 0) || (theOperand1 > 65535))
                    throw new RuntimeException("out of range variable");
                if ((theOperand2 < -32768) || (theOperand2 > 32767))
                    throw new RuntimeException("out of range increment");
            }
            if ((theOperand1 > 255)
                    || (theOperand2 < -128) || (theOperand2 > 127)) {
                addToCodeBuffer(ByteCode.WIDE);
                addToCodeBuffer(ByteCode.IINC);
                addToCodeBuffer((byte)(theOperand1 >> 8));
                addToCodeBuffer((byte)theOperand1);
                addToCodeBuffer((byte)(theOperand2 >> 8));
                addToCodeBuffer((byte)theOperand2);
            }
            else {
                addToCodeBuffer(ByteCode.WIDE);
                addToCodeBuffer(ByteCode.IINC);
                addToCodeBuffer((byte)theOperand1);
                addToCodeBuffer((byte)theOperand2);
            }
        }
        else {
            if (theOpCode == ByteCode.MULTIANEWARRAY) {
                if (DEBUG) {
                    if ((theOperand1 < 0) || (theOperand1 > 65535))
                        throw new RuntimeException("out of range index");
                    if ((theOperand2 < 0) || (theOperand2 > 255))
                        throw new RuntimeException("out of range dimensions");
                }
                addToCodeBuffer(ByteCode.MULTIANEWARRAY);
                addToCodeBuffer((byte)(theOperand1 >> 8));
                addToCodeBuffer((byte)theOperand1);
                addToCodeBuffer((byte)theOperand2);
            }
            else {
                throw new RuntimeException("Unexpected opcode for 2 operands");
            }
        }
    }

    public void add(byte theOpCode, String className) {
        if (DEBUGCODE)
            System.out.println("Add " + Integer.toHexString(theOpCode & 0xFF)
                                    + ", " + className);
        itsStackTop += ByteCode.stackChange[theOpCode & 0xFF];
        if (DEBUGSTACK) {
            System.out.println("After " + Integer.toHexString(theOpCode & 0xFF) + " stack = " + itsStackTop);
        }
        if (DEBUG) {
            if (itsStackTop < 0)
                throw new RuntimeException("Stack underflow");
        }
        switch (theOpCode) {
            case ByteCode.NEW :
            case ByteCode.ANEWARRAY :
            case ByteCode.CHECKCAST :
            case ByteCode.INSTANCEOF : {
                    short classIndex = itsConstantPool.addClass(className);
                    addToCodeBuffer(theOpCode);
                    addToCodeBuffer((byte)(classIndex >> 8));
                    addToCodeBuffer((byte)classIndex);
                }
                break;

            default :
                throw new RuntimeException("bad opcode for class reference");
        }
        if (itsStackTop > itsMaxStack) itsMaxStack = itsStackTop;
    }
    

    public void add(byte theOpCode, String className, String fieldName, 
                    String fieldType)
    {
        if (DEBUGCODE)
            System.out.println("Add " + Integer.toHexString(theOpCode & 0xFF)
                                    + ", " + className + ", " + fieldName + ", " + fieldType);
        itsStackTop += ByteCode.stackChange[theOpCode & 0xFF];
        if (DEBUG) {
            if (itsStackTop < 0)
                throw new RuntimeException("After " + Integer.toHexString(theOpCode & 0xFF) + " Stack underflow");
        }
        char fieldTypeChar = fieldType.charAt(0);
        int fieldSize = ((fieldTypeChar == 'J')
                                || (fieldTypeChar == 'D')) ? 2 : 1;
        switch (theOpCode) {
            case ByteCode.GETFIELD :
            case ByteCode.GETSTATIC :
                itsStackTop += fieldSize;
                break;
            case ByteCode.PUTSTATIC :
            case ByteCode.PUTFIELD :
                itsStackTop -= fieldSize;
                break;
            default :
                throw new RuntimeException("bad opcode for field reference");
        }
        short fieldRefIndex = itsConstantPool.addFieldRef(className,
                                             fieldName, fieldType);
        addToCodeBuffer(theOpCode);
        addToCodeBuffer((byte)(fieldRefIndex >> 8));
        addToCodeBuffer((byte)fieldRefIndex);

        if (itsStackTop > itsMaxStack) itsMaxStack = itsStackTop;
        if (DEBUGSTACK) {
            System.out.println("After " + Integer.toHexString(theOpCode & 0xFF) + " stack = " + itsStackTop);
        }
    }
    
    public void add(byte theOpCode, String className, String methodName,
                    String parametersType, String returnType)
    {
        if (DEBUGCODE)
            System.out.println("Add " + Integer.toHexString(theOpCode & 0xFF)
                                    + ", " + className + ", " + methodName + ", " + parametersType + ", " + returnType);
        int parameterInfo = sizeOfParameters(parametersType);
        itsStackTop -= (parameterInfo & 0xFFFF);
        itsStackTop += ByteCode.stackChange[theOpCode & 0xFF];     // adjusts for 'this'
        if (DEBUG) {
            if (itsStackTop < 0)
                throw new RuntimeException("After " + Integer.toHexString(theOpCode & 0xFF) + " Stack underflow");
        }
        if (itsStackTop > itsMaxStack) itsMaxStack = itsStackTop;

        switch (theOpCode) {
            case ByteCode.INVOKEVIRTUAL :
            case ByteCode.INVOKESPECIAL :
            case ByteCode.INVOKESTATIC :
            case ByteCode.INVOKEINTERFACE : {
                    char returnTypeChar = returnType.charAt(0);
                    if (returnTypeChar != 'V')
                        if ((returnTypeChar == 'J') || (returnTypeChar == 'D'))
                            itsStackTop += 2;
                        else
                            itsStackTop++;
                    addToCodeBuffer(theOpCode);
                    if (theOpCode == ByteCode.INVOKEINTERFACE) {
                        short ifMethodRefIndex
                                    = itsConstantPool.addInterfaceMethodRef(
                                               className, methodName,
                                               parametersType + returnType);
                        addToCodeBuffer((byte)(ifMethodRefIndex >> 8));
                        addToCodeBuffer((byte)ifMethodRefIndex);
                        addToCodeBuffer((byte)((parameterInfo >> 16) + 1));
                        addToCodeBuffer((byte)0);
                    }
                    else {
                        short methodRefIndex = itsConstantPool.addMethodRef(
                                               className, methodName,
                                               parametersType + returnType);
                        addToCodeBuffer((byte)(methodRefIndex >> 8));
                        addToCodeBuffer((byte)methodRefIndex);
                    }
                }
                break;

            default :
                throw new RuntimeException("bad opcode for method reference");
        }
        if (itsStackTop > itsMaxStack) itsMaxStack = itsStackTop;
        if (DEBUGSTACK) {
            System.out.println("After " + Integer.toHexString(theOpCode & 0xFF) + " stack = " + itsStackTop);
        }
    }
    
    public int markLabel(int theLabel) {
        return super.markLabel(theLabel, (short)itsCodeBufferTop);
    }

    public int markLabel(int theLabel, short stackTop) {
        itsStackTop = stackTop;
        return super.markLabel(theLabel, (short)itsCodeBufferTop);
    }

    public int markHandler(int theLabel) {
        itsStackTop = 1;
        return markLabel(theLabel);
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
        itsStackTop += delta;
        if (DEBUGSTACK) {
            System.out.println("After " + "adjustStackTop("+delta+")" + " stack = " + itsStackTop);
        }
        if (DEBUG) {
            if (itsStackTop < 0)
                throw new RuntimeException("Stack underflow");
        }
        if (itsStackTop > itsMaxStack) itsMaxStack = itsStackTop;
    }


    public void addToCodeBuffer(byte b) {
        if (DEBUG) {
            if (itsCurrentMethod == null)
                throw new RuntimeException("No method to add to");
        }
        if (itsCodeBuffer == null) {
            itsCodeBuffer = new byte[CodeBufferSize];
            itsCodeBuffer[0] = b;
            itsCodeBufferTop = 1;
        }
        else {
            if (itsCodeBufferTop == itsCodeBuffer.length) {
                byte currentBuffer[] = itsCodeBuffer;
                itsCodeBuffer = new byte[itsCodeBufferTop * 2];
                System.arraycopy(currentBuffer, 0, itsCodeBuffer,
                                                0, itsCodeBufferTop);
            }
            itsCodeBuffer[itsCodeBufferTop++] = b;
        }
    }
        
    public void addExceptionHandler(int startLabel, int endLabel,
                                    int handlerLabel, String catchClassName)
    {
        if ((startLabel & 0x80000000) != 0x80000000)
            throw new RuntimeException("Bad startLabel");
        if ((endLabel & 0x80000000) != 0x80000000)
            throw new RuntimeException("Bad endLabel");
        if ((handlerLabel & 0x80000000) != 0x80000000)
            throw new RuntimeException("Bad handlerLabel");
        
        /*
         * If catchClassName is null, use 0 for the catch_type_index; which 
         * means catch everything.  (Even when the verifier has let you throw
         * something other than a Throwable.)
         */
        ExceptionTableEntry newEntry
                        = new ExceptionTableEntry(
                                    startLabel,
                                    endLabel,
                                    handlerLabel,
                                    catchClassName == null
                                        ? 0
                                        : itsConstantPool.addClass(catchClassName));
        
        if (itsExceptionTable == null) {
            itsExceptionTable = new ExceptionTableEntry[ExceptionTableSize];
            itsExceptionTable[0] = newEntry;
            itsExceptionTableTop = 1;
        }
        else {
            if (itsExceptionTableTop == itsExceptionTable.length) {
                ExceptionTableEntry oldTable[] = itsExceptionTable;
                itsExceptionTable = new ExceptionTableEntry
                                            [itsExceptionTableTop * 2];
                System.arraycopy(oldTable, 0, itsExceptionTable,
                                                0, itsExceptionTableTop);
            }
            itsExceptionTable[itsExceptionTableTop++] = newEntry;
        }
    
    }

    public void addLineNumberEntry(short lineNumber) {
        if (DEBUG) {
            if (itsCurrentMethod == null)
                throw new RuntimeException("No method to stop");
        }

        if (itsLineNumberTable == null) {
            itsLineNumberTable = new int[LineNumberTableSize];
            itsLineNumberTable[0] = (itsCodeBufferTop << 16) + lineNumber;
            itsLineNumberTableTop = 1;
        }
        else {
            if (itsLineNumberTableTop == itsLineNumberTable.length) {
                int[] oldTable = itsLineNumberTable;
                itsLineNumberTable = new int[itsLineNumberTableTop * 2];
                System.arraycopy(oldTable, 0, itsLineNumberTable,
                                                0, itsLineNumberTableTop);
            }
            itsLineNumberTable[itsLineNumberTableTop++]
                                        = (itsCodeBufferTop << 16) + lineNumber;
        }
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
    	DataOutputStream out = new DataOutputStream(oStream);
        
        short sourceFileAttributeNameIndex = 0;
        if (itsSourceFileNameIndex != 0)
            sourceFileAttributeNameIndex
                        = itsConstantPool.addUtf8("SourceFile");

        out.writeLong(FileHeaderConstant);
        itsConstantPool.write(out);
        out.writeShort(itsFlags);
        out.writeShort(itsThisClassIndex);
        out.writeShort(itsSuperClassIndex);     
        out.writeShort(itsInterfaces.size());
        for (int i = 0; i < itsInterfaces.size(); i++) {
            out.writeShort(((Short)(itsInterfaces.elementAt(i))).shortValue());
        }
        out.writeShort(itsFields.size());
        for (int i = 0; i < itsFields.size(); i++) {
            ((ClassFileField)(itsFields.elementAt(i))).write(out);
        }
        out.writeShort(itsMethods.size());
        for (int i = 0; i < itsMethods.size(); i++) {
            ((ClassFileMethod)(itsMethods.elementAt(i))).write(out);
        }
        if (itsSourceFileNameIndex != 0) {
            out.writeShort(1);      // attributes count
            out.writeShort(sourceFileAttributeNameIndex);
            out.writeInt(2);
            out.writeShort(itsSourceFileNameIndex);
        }
        else
            out.writeShort(0);      // no attributes
    }
    
    /*
        Really weird. Returns an int with # parameters in hi 16 bits, and
        # slots occupied by parameters in the low 16 bits. If Java really
        supported references we wouldn't have to be this perverted.
    */
    private int sizeOfParameters(String pString)
    {
        if (DEBUG) {
            if (pString.charAt(0) != '(')
                throw new RuntimeException("Bad parameter signature");
        }
        int index = 1;
        int size = 0;
        int count = 0;
        while (pString.charAt(index) != ')') {
            switch (pString.charAt(index)) {
                case 'J' :
                case 'D' :
                    size++;
                    // fall thru
                case 'B' :
                case 'S' :
                case 'C' :
                case 'I' :
                case 'Z' :
                case 'F' :
                    size++;
                    count++;
                    index++;
                    break;
                case '[' :
                    while (pString.charAt(index) == '[') index++;
                    if (pString.charAt(index) != 'L') {
                        size++;
                        count++;
                        index++;
                        break;
                    }
                        // fall thru
                case 'L' :
                    size++;
                    count++;
                    while (pString.charAt(index++) != ';') ;
                    break;
                default :
                    throw new RuntimeException("Bad signature character");
            }
        }
        return ((count << 16) | size);
    }

    private static final int LineNumberTableSize = 16;
    private static final int ExceptionTableSize = 4;

    private final static long FileHeaderConstant = 0xCAFEBABE0003002DL;
    // Set DEBUG flags to true to get better checking and progress info.
    private static final boolean DEBUG = false;
    private static final boolean DEBUGSTACK = false;
    private static final boolean DEBUGLABELS = false;
    private static final boolean DEBUGCODE = false;
    private static final int CodeBufferSize = 128;

    private ExceptionTableEntry itsExceptionTable[];
    private int itsExceptionTableTop;

    private int itsLineNumberTable[];   // pack start_pc & line_number together
    private int itsLineNumberTableTop;

    private byte itsCodeBuffer[];
    private int itsCodeBufferTop;
    
    private ConstantPool itsConstantPool;

    private short itsSourceFileAttributeIndex;

    private ClassFileMethod itsCurrentMethod;
    private short itsStackTop;

    private short itsMaxStack;
    private short itsMaxLocals;
    
    private Vector itsMethods = new Vector();
    private Vector itsFields = new Vector();
    private Vector itsInterfaces = new Vector();

    private short itsFlags;
    private short itsThisClassIndex;
    private short itsSuperClassIndex;
    private short itsSourceFileNameIndex;

}

class ExceptionTableEntry {
    
    ExceptionTableEntry(int startLabel, int endLabel, 
                            int handlerLabel, short catchType)
    {
        itsStartLabel = startLabel;
        itsEndLabel = endLabel;
        itsHandlerLabel = handlerLabel;
        itsCatchType = catchType;
    }
    
    short getStartPC(Label labelTable[])
    {
        short pc = labelTable[itsStartLabel & 0x7FFFFFFF].getPC();
        if (pc == -1)
            throw new RuntimeException("start label not defined");
        return pc;
    }
    
    short getEndPC(Label labelTable[])
    {
        short pc = labelTable[itsEndLabel & 0x7FFFFFFF].getPC();
        if (pc == -1)
            throw new RuntimeException("end label not defined");
        return pc;
    }
    
    short getHandlerPC(Label labelTable[])
    {
        short pc = labelTable[itsHandlerLabel & 0x7FFFFFFF].getPC();
        if (pc == -1)
            throw new RuntimeException("handler label not defined");
        return pc;
    }
    
    short getCatchType()
    {
        return itsCatchType;
    }
    
    private int itsStartLabel;
    private int itsEndLabel;
    private int itsHandlerLabel;
    private short itsCatchType;
}

class ClassFileField {

    ClassFileField(short nameIndex, short typeIndex, short flags)
    {
        itsNameIndex = nameIndex;
        itsTypeIndex = typeIndex;
        itsFlags = flags;
    }

    ClassFileField(short nameIndex, short typeIndex, short flags, short cvAttr[])
    {
        itsNameIndex = nameIndex;
        itsTypeIndex = typeIndex;
        itsFlags = flags;
        itsAttr = cvAttr;
    }

    void write(DataOutputStream out) throws IOException
    {
        out.writeShort(itsFlags);
        out.writeShort(itsNameIndex);
        out.writeShort(itsTypeIndex);
        if (itsAttr == null)
            out.writeShort(0);              // no attributes
        else {
            out.writeShort(1);
            out.writeShort(itsAttr[0]);
            out.writeShort(itsAttr[1]);
            out.writeShort(itsAttr[2]);
            out.writeShort(itsAttr[3]);
        }
    }

    private short itsNameIndex;
    private short itsTypeIndex;
    private short itsFlags;
    private short itsAttr[];
}

class ClassFileMethod {

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
    
    void write(DataOutputStream out) throws IOException
    {
        out.writeShort(itsFlags);
        out.writeShort(itsNameIndex);
        out.writeShort(itsTypeIndex);
        out.writeShort(1);              // Code attribute only
        out.write(itsCodeAttribute, 0, itsCodeAttribute.length);
    }

    private short itsNameIndex;
    private short itsTypeIndex;
    private short itsFlags;
    private byte[] itsCodeAttribute;

}

class ConstantPool {

    ConstantPool()
    {
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

    void write(DataOutputStream out) throws IOException
    {
        out.writeShort((short)(itsTopIndex));
        out.write(itsPool, 0, itsTop);
    }

    short addConstant(int k)
    {
        ensure(5);
        itsPool[itsTop++] = CONSTANT_Integer;
        itsPool[itsTop++] = (byte)(k >> 24);
        itsPool[itsTop++] = (byte)(k >> 16);
        itsPool[itsTop++] = (byte)(k >> 8);
        itsPool[itsTop++] = (byte)k;
        return (short)(itsTopIndex++);
    }
    
    short addConstant(long k)
    {
        ensure(9);
        itsPool[itsTop++] = CONSTANT_Long;
        itsPool[itsTop++] = (byte)(k >> 56);
        itsPool[itsTop++] = (byte)(k >> 48);
        itsPool[itsTop++] = (byte)(k >> 40);
        itsPool[itsTop++] = (byte)(k >> 32);
        itsPool[itsTop++] = (byte)(k >> 24);
        itsPool[itsTop++] = (byte)(k >> 16);
        itsPool[itsTop++] = (byte)(k >> 8);
        itsPool[itsTop++] = (byte)k;
        short index = (short)(itsTopIndex);
        itsTopIndex += 2;
        return index;
    }
    
    short addConstant(float k)
    {
        ensure(5);
        itsPool[itsTop++] = CONSTANT_Float;
        int bits = Float.floatToIntBits(k);
        itsPool[itsTop++] = (byte)(bits >> 24);
        itsPool[itsTop++] = (byte)(bits >> 16);
        itsPool[itsTop++] = (byte)(bits >> 8);
        itsPool[itsTop++] = (byte)bits;
        return (short)(itsTopIndex++);
    }
    
    short addConstant(double k)
    {
        ensure(9);
        itsPool[itsTop++] = CONSTANT_Double;
        long bits = Double.doubleToLongBits(k);
        itsPool[itsTop++] = (byte)(bits >> 56);
        itsPool[itsTop++] = (byte)(bits >> 48);
        itsPool[itsTop++] = (byte)(bits >> 40);
        itsPool[itsTop++] = (byte)(bits >> 32);
        itsPool[itsTop++] = (byte)(bits >> 24);
        itsPool[itsTop++] = (byte)(bits >> 16);
        itsPool[itsTop++] = (byte)(bits >> 8);
        itsPool[itsTop++] = (byte)bits;
        short index = (short)(itsTopIndex);
        itsTopIndex += 2;
        return index;
    }
    
    short addConstant(String k)
    {
        Utf8StringIndexPair theIndex = (Utf8StringIndexPair)(itsUtf8Hash.get(k));
        if (theIndex == null) {
            addUtf8(k);
            theIndex = (Utf8StringIndexPair)(itsUtf8Hash.get(k));   // OPT
        }
        if (theIndex.itsStringIndex == -1) {
            theIndex.itsStringIndex = (short)(itsTopIndex++);
            ensure(3);
            itsPool[itsTop++] = CONSTANT_String;
            itsPool[itsTop++] = (byte)(theIndex.itsUtf8Index >> 8);
            itsPool[itsTop++] = (byte)theIndex.itsUtf8Index;
        }
        return theIndex.itsStringIndex;
    }
    
    short addUtf8(String contents)
    {
        Utf8StringIndexPair theIndex = (Utf8StringIndexPair)(itsUtf8Hash.get(contents));
        if (theIndex == null) {
            theIndex = new Utf8StringIndexPair((short)(itsTopIndex++), (short)(-1));
            itsUtf8Hash.put(contents, theIndex);
            try {
                // using DataOutputStream.writeUTF is a lot faster than String.getBytes("UTF8")
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                DataOutputStream dos = new DataOutputStream(baos);
                dos.writeUTF(contents);
                byte theBytes[] = baos.toByteArray();
                ensure(1 + theBytes.length);
                itsPool[itsTop++] = CONSTANT_Utf8;
                System.arraycopy(theBytes, 0, itsPool, itsTop, theBytes.length);
                itsTop += theBytes.length;
            }
            catch (IOException iox) {
                throw WrappedException.wrapException(iox);
            }
        }
        return theIndex.itsUtf8Index;
    }

    short addNameAndType(short nameIndex, short typeIndex)
    {
        ensure(5);
        itsPool[itsTop++] = CONSTANT_NameAndType;
        itsPool[itsTop++] = (byte)(nameIndex >> 8);
        itsPool[itsTop++] = (byte)(nameIndex);
        itsPool[itsTop++] = (byte)(typeIndex >> 8);
        itsPool[itsTop++] = (byte)(typeIndex);
        return (short)(itsTopIndex++);
    }

    short addClass(short classIndex)
    {
        Short classIndexKey = new Short(classIndex);
        Short theIndex = (Short)(itsClassHash.get(classIndexKey));
        if (theIndex == null) {
            ensure(3);
            itsPool[itsTop++] = CONSTANT_Class;
            itsPool[itsTop++] = (byte)(classIndex >> 8);
            itsPool[itsTop++] = (byte)(classIndex);
            theIndex = new Short((short)(itsTopIndex++));
            itsClassHash.put(classIndexKey, theIndex);
        }
       return theIndex.shortValue();
     }

    short addClass(String className)
    {
        short classIndex
                = addUtf8(ClassFileWriter.fullyQualifiedForm(className));
        return addClass(classIndex);
    }

    short addFieldRef(String className, String fieldName, String fieldType)
    {
        String fieldRefString = className + " " + fieldName + " " + fieldType;
        Short theIndex = (Short)(itsFieldRefHash.get(fieldRefString));
        if (theIndex == null) {
            short nameIndex = addUtf8(fieldName);
            short typeIndex = addUtf8(fieldType);
            short ntIndex = addNameAndType(nameIndex, typeIndex);
            short classIndex = addClass(className);
            ensure(5);
            itsPool[itsTop++] = CONSTANT_Fieldref;
            itsPool[itsTop++] = (byte)(classIndex >> 8);
            itsPool[itsTop++] = (byte)(classIndex);
            itsPool[itsTop++] = (byte)(ntIndex >> 8);
            itsPool[itsTop++] = (byte)(ntIndex);
            theIndex = new Short((short)(itsTopIndex++));
            itsFieldRefHash.put(fieldRefString, theIndex);
        }
        return theIndex.shortValue();
    }

    short addMethodRef(String className, String methodName, String fieldType)
    {
        String methodRefString = className + " " + methodName + " " + fieldType;
        Short theIndex = (Short)(itsMethodRefHash.get(methodRefString));
        if (theIndex == null) {
            short nameIndex = addUtf8(methodName);
            short typeIndex = addUtf8(fieldType);
            short ntIndex = addNameAndType(nameIndex, typeIndex);
            short classIndex = addClass(className);
            ensure(5);
            itsPool[itsTop++] = CONSTANT_Methodref;
            itsPool[itsTop++] = (byte)(classIndex >> 8);
            itsPool[itsTop++] = (byte)(classIndex);
            itsPool[itsTop++] = (byte)(ntIndex >> 8);
            itsPool[itsTop++] = (byte)(ntIndex);
            theIndex = new Short((short)(itsTopIndex++));
            itsMethodRefHash.put(methodRefString, theIndex);
        }
        return theIndex.shortValue();
    }

    short addInterfaceMethodRef(String className,
                                        String methodName, String methodType)
    {
        short nameIndex = addUtf8(methodName);
        short typeIndex = addUtf8(methodType);
        short ntIndex = addNameAndType(nameIndex, typeIndex);
        short classIndex = addClass(className);
        ensure(5);
        itsPool[itsTop++] = CONSTANT_InterfaceMethodref;
        itsPool[itsTop++] = (byte)(classIndex >> 8);
        itsPool[itsTop++] = (byte)(classIndex);
        itsPool[itsTop++] = (byte)(ntIndex >> 8);
        itsPool[itsTop++] = (byte)(ntIndex);
        return (short)(itsTopIndex++);
    }

    void ensure(int howMuch)
    {
        while ((itsTop + howMuch) >= itsPool.length) {
            byte oldPool[] = itsPool;
            itsPool = new byte[itsPool.length * 2];
            System.arraycopy(oldPool, 0, itsPool, 0, itsTop);
        }
    }

    private Hashtable itsUtf8Hash = new Hashtable();
    private Hashtable itsFieldRefHash = new Hashtable();
    private Hashtable itsMethodRefHash = new Hashtable();
    private Hashtable itsClassHash = new Hashtable();
    
    private int itsTop;
    private int itsTopIndex;
    private byte itsPool[];
}

class Utf8StringIndexPair {
    
    Utf8StringIndexPair(short utf8Index, short stringIndex)
    {
        itsUtf8Index = utf8Index;
        itsStringIndex = stringIndex;
    }
    
    short itsUtf8Index;
    short itsStringIndex;
}

