/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * This program can read a set of classfiles and display the dependcies of
 * those classfiles on other classfiles. It can optionally take lists of
 * independent classfiles to ignore. It will return a value -1 on error or 
 * the count of unexpected dependencies (zero if none).
 *
 * This is based on code hacked out of netscape.tools.DumpClass
 *
 * jband - 09/06/98
 */

package com.netscape.jsdebugging.tools.depend;

import java.util.Hashtable;
import java.util.Enumeration;
import java.util.Vector;

import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.FilenameFilter;
import java.io.PrintStream;

class Main {
    public static void main(String[] argv) {
        new Main().go(argv);
    }

    Main() {
        _classinfos = new Hashtable();
        _inputClasses = new Vector();
        _ignoreStrings= new Vector();
    }

    void usage() {

        System.out.println("usage: Main [-a] [-i string] name [more_names...]");
        System.out.println("        -a means \'all\'");
        System.out.println("        -i means \'ignore\'");
        System.out.println("example:");
        System.out.println("  Main -i java/io -i java/lang *.class foo/*");
    }

    void go(String[] argv) {
        int retval = 0;
        if (argv.length < 1) {
            usage();
            System.exit(-1);
            return;
        }
        try {
            parseCommandline(argv);

            for(int i = 0; i < _inputClasses.size(); i++) {
//                System.out.println(_inputClasses.elementAt(i));
                readClass((String)_inputClasses.elementAt(i));
            }

            Enumeration e = getAllClassInfos();
            Vector dep = new Vector();

            int i;
            int size;

            
next_el:    while(e.hasMoreElements()) {
                ClassInfo info = (ClassInfo) e.nextElement();
                String name = info._name;

                if(!_all) {
                    // remove input classnames
                    size = _inputClasses.size();
                    for(i = 0; i < size; i++) {
                        String cur = (String) _inputClasses.elementAt(i);
                        if(name.equals(cur))
                            continue next_el;
                    }
                    // remove explicit ignore classnames
                    size = _ignoreStrings.size();
                    for(i = 0; i < size; i++) {
                        String cur = (String) _ignoreStrings.elementAt(i);
                        if(name.startsWith(cur))
                            continue next_el;
                    }
                }

                // slow insertion sort...
                size = dep.size();
                for(i = 0; i < size; i++) {
                    ClassInfo cur = (ClassInfo) dep.elementAt(i);
                    if( 0 > name.compareTo(cur._name)) {
                        dep.insertElementAt(info,i);
                        break;
                    }
                }
                if(i == size)
                    dep.addElement(info);
                retval++ ;
            }

            size = dep.size();
            for(i = 0; i < size; i++) {
                ClassInfo cur = (ClassInfo) dep.elementAt(i);
                System.out.println(cur._name);
            }

        } catch (Throwable t) {
            System.err.println("### Problem in depend: ");
            t.printStackTrace();
            System.exit(-1);
        }
        System.exit(retval);
    }

    String backToSlash(String name) {
        if(-1 == name.indexOf('\\'))
            return name;
        return name.replace('\\', '/');
    }

    void parseCommandline(String[] argv) throws Exception {
        for(int i = 0; i < argv.length; i++) {
            String arg = argv[i];
            if("-a".equals(arg))
                _all = true;
            else if("-i".equals(arg)) {
                if(++i == argv.length)
                    throw new Exception("-i flag not followed by class or package");
                _ignoreStrings.addElement(backToSlash(argv[i]));
            }
            else
                addInputClass(backToSlash(arg));
        }
    }

    void addInputClass(String str) {
        String tmp = new String(str).toLowerCase();
        if(tmp.endsWith(".class"))
            str = str.substring(0, str.length()-6);

        if(str.endsWith("*"))
            addWildCardsToInputClassesList(str.substring(0, str.length()-1));
        else
            _inputClasses.addElement(str);
    }

    void addWildCardsToInputClassesList(String str) {
        String pkg = new String(str);
        if(0 == str.length())
            str = ".";

        String[] files = new File(str).list(new ClassnameFileFilter());
        int len;
        if(null != files && 0 != (len = files.length))
            for(int i = 0; i < len; i++) {
                String str1 = files[i];
                str1 = str1.substring(0, str1.length()-6);
                _inputClasses.addElement(pkg+str1);
            }
    }

    void readClass(String filename) throws Exception {
        int i;
        int len;
        ClassReader cr = new ClassReader(filename+".class");
        cr.read();
        ClassInfo info = getClassInfo(cr.getName());

        info._super = getClassInfo(cr.getSuperName());

        String[] interfaceNames = cr.getInterfaces();
        if(null != interfaceNames && 0 != (len = interfaceNames.length)) {
            info._interfaces = new ClassInfo[len];
            for(i = 0; i < len; i++)
                info._interfaces[i] = getClassInfo(interfaceNames[i]);
        }

        String[] usesNames = cr.getUses();
        if(null != usesNames && 0 != (len = usesNames.length)) {
            info._uses = new ClassInfo[len];
            for(i = 0; i < len; i++)
                info._uses[i] = getClassInfo(usesNames[i]);
        }

        info._read = true;
    }

    ClassInfo getClassInfo(String name) {
        ClassInfo info;
        info = (ClassInfo) _classinfos.get(name);
        if(null == info) {
            info = new ClassInfo(name);
            _classinfos.put(name, info);
        }
        return info;
    }

    Enumeration getAllClassInfos() {
        return _classinfos.elements();
    }

    Hashtable _classinfos;
    Vector    _inputClasses;
    Vector    _ignoreStrings;
    boolean   _all;
}

class ClassInfo {
    ClassInfo(String name) {_name = name;}
    String      _name;
    ClassInfo   _super;
    ClassInfo[] _interfaces;
    ClassInfo[] _uses;
    boolean     _read;
}

class ClassReader {
    static final int MAGIC = 0xCAFEBABE;
    static final int MAJOR_VERSION = 45;
    static final int MINOR_VERSION = 3;

    // access_flags
    static final int ACC_PUBLIC         = 0x0001;
    static final int ACC_PRIVATE        = 0x0002;
    static final int ACC_PROTECTED      = 0x0004;
    static final int ACC_STATIC         = 0x0008;
    static final int ACC_FINAL          = 0x0010;
    static final int ACC_SYNCHRONIZED   = 0x0020;
    static final int ACC_THREADSAFE     = 0x0040;
    static final int ACC_TRANSIENT      = 0x0080;
    static final int ACC_NATIVE         = 0x0100;
    static final int ACC_INTERFACE      = 0x0200;
    static final int ACC_ABSTRACT       = 0x0400;

    // constant types
    static final int CONSTANT_Class             = 7;
    static final int CONSTANT_Fieldref          = 9;
    static final int CONSTANT_Methodref         = 10;
    static final int CONSTANT_String            = 8;
    static final int CONSTANT_Integer           = 3;
    static final int CONSTANT_Float             = 4;
    static final int CONSTANT_Long              = 5;
    static final int CONSTANT_Double            = 6;
    static final int CONSTANT_InterfaceMethodref= 11;
    static final int CONSTANT_NameAndType       = 12;
    static final int CONSTANT_Asciz             = 1;


    ClassReader(String filename) {
        _filename = filename;
        _usesTable = new Hashtable();
        _classNames = new Vector();
    }

    void read()  throws Exception {
        LoggingInputStream s = new LoggingInputStream(new FileInputStream(_filename));

        int magic = s.readInt("magic number");
        if (magic != MAGIC)
            throw new Exception("The file " + _filename + " isn't a Java .class file.");

        int minorVersion = s.readShort("minorVersion");
        int majorVersion = s.readShort("majorVersion");
        if (majorVersion != MAJOR_VERSION || minorVersion != MINOR_VERSION)
            throw new Exception("The file " + _filename +
                                " has the wrong version (" +
                                majorVersion + "." + minorVersion +
                                " rather than " +
                                MAJOR_VERSION + "." + MINOR_VERSION + ")");

        short constant_pool_count = s.readShort("constant_pool_count");
        cpool = new Object[constant_pool_count];

        // suck in the constant pool for future use
        for (int i = 1; i < constant_pool_count; i++) { // constant_pool[0] always unused
            byte type = s.readByte("constant pool [" + i + "] item type");
            switch (type) {
              case CONSTANT_Class: {
                  short name_index = s.readShort("Class name_index");
                  cpool[i] = new ClassStruct(name_index);
                  _classNames.addElement(new Integer(name_index));
                  break;
              }
              case CONSTANT_Fieldref: {
                  short class_index = s.readShort("Fieldref class_index");
                  short name_and_type_index = s.readShort("Fieldref name_and_type_index");
                  cpool[i] = new FieldrefStruct(class_index, name_and_type_index);
                  break;
              }
              case CONSTANT_Methodref: {
                  short class_index = s.readShort("Methodref class_index");
                  short name_and_type_index = s.readShort("Methodref name_and_type_index");
                  cpool[i] = new MethodrefStruct(class_index, name_and_type_index);
                  break;
              }
              case CONSTANT_InterfaceMethodref: {
                  short class_index = s.readShort("InterfaceMethodref class_index");
                  short name_and_type_index = s.readShort("InterfaceMethodref name_and_type_index");
                  cpool[i] = new InterfaceMethodrefStruct(class_index, name_and_type_index);
                  break;
              }
              case CONSTANT_String: {
                  short string_index = s.readShort("String string_index");
                  cpool[i] = new StringStruct(string_index);
                  break;
              }
              case CONSTANT_Integer: {
                  int value = s.readInt("Integer value");
                  cpool[i] = new Integer(value);
                  break;
              }
              case CONSTANT_Float: {
                  float value = s.readFloat("Float value");
                  cpool[i] = new Float(value);
                  break;
              }
              case CONSTANT_Long: {
                  long value = s.readLong("Long value");
                  i++;  /* Argh!!!!! */
                  cpool[i] = new Long(value);
                  break;
              }
              case CONSTANT_Double: {
                  double value = s.readDouble("Double value");
                  i++;  /* Argh!!!!! */
                  cpool[i] = new Double(value);
                  break;
              }
              case CONSTANT_NameAndType: {
                  short name_index = s.readShort("NameAndType name_index");
                  short signature_index = s.readShort("NameAndType signature_index");
                  cpool[i] = new NameAndTypeStruct(name_index, signature_index);
                  break;
              }
              case CONSTANT_Asciz: {
                  cpool[i] = s.readUTF("Asciz value");
                  break;
              }
              default:
                throw new Exception("Unrecognized constant pool type: " + type);
            }
        }

        short access_flags = s.readShort("access_flags");

        short this_class = s.readShort("this_class");
        ClassStruct myClass = (ClassStruct)cpool[this_class];
        if (myClass != null)
            _name = getName(myClass.name_index);

        short super_class = s.readShort("super_class");
        ClassStruct mySuper = (ClassStruct)cpool[super_class];
        if (mySuper != null)
            _supername = getName(mySuper.name_index);


        short interfaces_count = s.readShort("interfaces_count");
        if (interfaces_count != 0) {
            _interfaces = new String[interfaces_count];
            for (int i = 0; i < interfaces_count; i++) {
                short index = s.readShort("Interface index");
                _interfaces[i] = getName(((ClassStruct)cpool[index]).name_index);
            }
        }

        short fields_count = s.readShort("fields_count");
        for (int i = 0; i < fields_count; i++) {
            readField(s);
        }


        short methods_count = s.readShort("methods_count");
        for (int i = 0; i < methods_count; i++) {
            readMethod(s);
        }

        short attributes_count = s.readShort("attributes_count");
        for (int i = 0; i < attributes_count; i++) {
            readAttribute(s);
        }

        finishUses();
    }


    void readField(LoggingInputStream s) throws Exception {
        short access_flags = s.readShort("Field access_flags");
        short name_index = s.readShort("Field name_index");
        short signature_index = s.readShort("Field signature_index");
        short attributes_count = s.readShort("Field attributes_count");

        addSig(getName(signature_index));
        for (int i = 0; i < attributes_count; i++) {
            readAttribute(s);
        }
    }


    void readMethod(LoggingInputStream s) throws Exception {
        short access_flags = s.readShort("Method access_flags");
        short name_index = s.readShort("Method name_index");
        short signature_index = s.readShort("Method signature_index");
        short attributes_count = s.readShort("Method attributes_count");

        addSig(getName(signature_index));
        for (int i = 0; i < attributes_count; i++) {
            readAttribute(s);
        }
    }

    void readAttribute(LoggingInputStream s) throws Exception {
        short attribute_name_index = s.readShort("Attribute  attribute_name_index");
        int attribute_length = s.readInt("Attribute attribute_length");
        String name = (String)cpool[attribute_name_index];

        if ("SourceFile".equals(name)) {
            short sourcefile_index = s.readShort("Attribute SourceFile sourcefile_index");
        }
        else if ("ConstantValue".equals(name)) {
            short constantvalue_index = s.readShort("Attribute ConstantValue constantvalue_index");
        }
        else if ("Code".equals(name)) {
            short max_stack = s.readShort("Attribute Code max_stack");
            short max_locals = s.readShort("Attribute Code max_locals");
            int code_length = s.readInt("Attribute Code code_length");

            byte[] code = new byte[code_length];
            s.read(code, "Attribute Code bytecodes");
            short exception_table_length = s.readShort("Attribute Code exception_table_length");

            if (exception_table_length != 0) {
                for (int i = 0; i < exception_table_length; i++) {
                    short start_pc = s.readShort("Attribute Code ExceptionTable start_pc");
                    short end_pc = s.readShort("Attribute Code ExceptionTable end_pc");
                    short handler_pc = s.readShort("Attribute Code ExceptionTable handler_pc");
                    short catch_type = s.readShort("Attribute Code ExceptionTable catch_type");
                }
            }

            short attributes_count = s.readShort("Attribute Code attributes_count");
            for (int i = 0; i < attributes_count; i++) {
                readAttribute(s);
            }
        }
        else if ("LineNumberTable".equals(name)) {
            short line_number_table_length = s.readShort("Attribute LineNumberTable line_number_table_length");

            if (line_number_table_length != 0) {
                for (int i = 0; i < line_number_table_length; i++) {
                    short start_pc = s.readShort("Attribute LineNumberTable start_pc");
                    short line_number = s.readShort("Attribute LineNumberTable line_number");
                }
            }
        }
        else if ("LocalVariableTable".equals(name)) {
            short local_variable_table_length = s.readShort("Attribute LocalVariableNumberTable local_variable_table_length");
            if (local_variable_table_length != 0) {
                for (int i = 0; i < local_variable_table_length; i++) {
                    short start_pc = s.readShort("Attribute LocalVariableNumberTable start_pc");
                    short length = s.readShort("Attribute LocalVariableNumberTable length");
                    short name_index = s.readShort("Attribute LocalVariableNumberTable name_index");
                    short signature_index = s.readShort("Attribute LocalVariableNumberTable signature_index");
                    short slot = s.readShort("Attribute LocalVariableNumberTable slot");

                    addSig(getName(signature_index));
                }
            }
        }
        else {
            s.skipBytes(attribute_length, "Unrecognized attribute");
        }
    }


    String getName(int index) {
        Object obj = cpool[index];
        if (obj == null)
            return null;
        else
            return obj.toString();
    }


    Object[] cpool;

    Hashtable _usesTable;
    Vector    _classNames;

    void addSig(String sig) {
//        System.out.println("sig: "+sig);
        int start, end, len, cur;
        if(null == sig || 0 == (len = sig.length()))
            return;
        for(cur = 0; cur < len; cur++) {
            if(sig.charAt(cur) == 'L') {
                start = ++cur;
                while(sig.charAt(++cur) != ';')
                    ;
                addUses(sig.substring(start,cur));
//                System.out.println("sig_added: "+sig.substring(start,cur)+ " from "+sig);
            }
        }
    }

    void addUses(String name) {
        if(null == name || 0 == name.length())
            return;
        if(name.charAt(0) =='[') {
            addSig(name);
            return;
        }
        _usesTable.put(name,name);
//        System.out.println("add_uses_added: "+name);
    }

    void finishUses() {
        int len;
        int i;

        // gather in all the names from the const pool CONSTANT_Class types
        len = _classNames.size();
        for(i = 0; i < len; i++)
            addUses(getName(((Integer)_classNames.elementAt(i)).intValue()));

        len = _usesTable.size();
        if(0 != len) {
            i = 0;
            _uses = new String[len];
            Enumeration e = _usesTable.keys();
            while(e.hasMoreElements())
                _uses[i++] = (String) e.nextElement();
        }
        _usesTable = null;
    }

    /*********************************/

    String   getName()        {return _name;}
    String   getSuperName()   {return _supername;}
    String[] getInterfaces()  {return _interfaces;}
    String[] getUses()        {return _uses;}


    String      _filename;
    String      _name;
    String      _supername;
    String[]    _interfaces;
    String[]    _uses;
}


class ConstantPoolStruct {
}

class ClassStruct extends ConstantPoolStruct {
    public short name_index;
    ClassStruct(short name_index) {
        this.name_index = name_index;
    }
}

class FieldrefStruct extends ConstantPoolStruct {
    public short class_index;
    public short name_and_type_index;
    FieldrefStruct(short class_index, short name_and_type_index) {
        this.class_index = class_index;
        this.name_and_type_index = name_and_type_index;
    }
}

class MethodrefStruct extends ConstantPoolStruct {
    public short class_index;
    public short name_and_type_index;
    MethodrefStruct(short class_index, short name_and_type_index) {
        this.class_index = class_index;
        this.name_and_type_index = name_and_type_index;
    }
}

class InterfaceMethodrefStruct extends ConstantPoolStruct {
    public short class_index;
    public short name_and_type_index;
    InterfaceMethodrefStruct(short class_index, short name_and_type_index) {
        this.class_index = class_index;
        this.name_and_type_index = name_and_type_index;
    }
}

class StringStruct extends ConstantPoolStruct {
    public short string_index;
    StringStruct(short string_index) {
        this.string_index = string_index;
    }
}

class NameAndTypeStruct extends ConstantPoolStruct {
    public short name_index;
    public short signature_index;
    NameAndTypeStruct(short name_index, short signature_index) {
        this.name_index = name_index;
        this.signature_index = signature_index;
    }
}

class LoggingInputStream {
    public boolean trace = false;
//    public boolean trace = true;
    DataInputStream s;
    PrintStream traceStream = System.out;
//    PrintStream traceStream = System.err;

    LoggingInputStream(InputStream s) {
        super();
        this.s = new DataInputStream(s);
    }

    public int readInt(String reason) throws IOException {
        if (trace) traceStream.print("# reading int for " + reason);
        int result = s.readInt();
        if (trace) traceStream.println(" => " + result);
        return result;
    }

    public short readShort(String reason) throws IOException {
        if (trace) traceStream.print("# reading short for " + reason);
        short result = s.readShort();
        if (trace) traceStream.println(" => " + result);
        return result;
    }

    public byte readByte(String reason) throws IOException {
        if (trace) traceStream.print("# reading byte for " + reason);
        byte result = s.readByte();
        if (trace) traceStream.println(" => " + result);
        return result;
    }

    public float readFloat(String reason) throws IOException {
        if (trace) traceStream.print("# reading float for " + reason);
        float result = s.readFloat();
        if (trace) traceStream.println(" => " + result);
        return result;
    }

    public long readLong(String reason) throws IOException {
        if (trace) traceStream.print("# reading long for " + reason);
        long result = s.readLong();
        if (trace) traceStream.println(" => " + result);
        return result;
    }

    public double readDouble(String reason) throws IOException {
        if (trace) traceStream.print("# reading double for " + reason);
        double result = s.readDouble();
        if (trace) traceStream.println(" => " + result);
        return result;
    }

    public int read(byte[] b, String reason) throws IOException {
        if (trace) traceStream.print("# reading byte[" + b.length + "] for " + reason);
        int result = s.read(b);
        if (trace) traceStream.println(" => " + result);
        return result;
    }

    public void skipBytes(int amount, String reason) throws IOException {
        if (trace) traceStream.println("# skipping " + amount + " bytes for " + reason);
        s.skipBytes(amount);
    }

    public String readUTF(String reason) throws IOException {
        if (trace) traceStream.print("# reading utf for " + reason);
        String result = DataInputStream.readUTF(s);
        if (trace) traceStream.println(" => " + result);
        return result;
    }
}


class ClassnameFileFilter implements FilenameFilter {
    public boolean accept(File dir, String name) {
        return name.endsWith(".class");
    }
}    