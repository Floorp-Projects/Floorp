/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
 Client QA Team, St. Petersburg, Russia
*/

import org.mozilla.xpcom.*;
import java.lang.reflect.*;
import java.io.DataOutputStream;
import java.io.FileOutputStream;
import java.util.Hashtable;

public class J2XINOUTClientTestComponent implements iJ2XINOUTClientTestComponent, iClientTestComponent, iJClientTestComponent, iExclusionSupport {
    private iJ2XINOUTServerTestComponent server = null;
    private String testLocation = null;
    private String logLocation = null;
    private Hashtable exclusionHash = new Hashtable();
    private byte endOfData = 112;
    

    public J2XINOUTClientTestComponent() {
        System.out.println("DEbug:avm:J2XINOUTClientTestComponent constructor");
    }

    private void printResult(String res,String fileName) {
        try{
            DataOutputStream f=new DataOutputStream(new FileOutputStream(logLocation+"/" + fileName)); 
            f.writeBytes(res);
            f.close();
        } catch(Exception e) {
            System.err.println("Exception during writing the file: " +e);
            e.printStackTrace();
        }
    }

    public void initialize(String serverProgID) {

        System.out.println("DEbug:avm:J2XINOUTClientTestComponent:initialize");
        //Really code from tHack should be here!!
    }
    public void exclude(int count, String[] exclusionList) {
        System.out.println("DEbug:avm:J2XINOUTClientTestComponent:exclude");
        for(int i=0;i<count;i++) {
            exclusionHash.put((Object)exclusionList[i],new Object());
        }
    }
    public void tHack(nsIComponentManager cm, String serverProgID) {
        System.out.println("DEbug:avm:J2XINOUTClientTestComponent:tHack");
        nsIFactory factory = null;
        
        if(cm == null) {
            System.out.println("DEbug:avm:ComponentManager is NULL!!!!!");
            return;
        }
        factory = cm.findFactory(J2XINOUTServerCID);
        if(factory == null) {
            System.out.println("DEbug:avm:Factory is NULL!!!!!");
            return;
        }
        Object res = factory.createInstance(null, iJ2XINOUTServerTestComponent.IID);
        if(res == null) {
            System.out.println("DEbug:avm:Instance is NULL!!!!!");
            return;
        }
        server = (iJ2XINOUTServerTestComponent)res;
        if(server == null) {
            System.err.println("Create instance failed!! Server is NULLLLLLLLLLLLLLLLLLL");
            return;
        }

        String[] ss = new String[1];
        String[] sss = new String[1];
        server.getTestLocation(ss,sss);
        testLocation = ss[0];
        logLocation = sss[0];
    } 

    /* void Execute (); */
    public void execute() {
        System.out.println("DEbug:avm:J2XINOUTClientTestComponent:execute");
        if(server == null) {
            System.err.println("Server is not initialized!!!");
            return;
        }
         if(!exclusionHash.containsKey("short"))
             testShort();
         if(!exclusionHash.containsKey("long"))
             testLong();
         if(!exclusionHash.containsKey("longlong"))
             testLonglong();
         if(!exclusionHash.containsKey("octet"))
             testByte();
         if(!exclusionHash.containsKey("ushort"))
             testUShort();
         if(!exclusionHash.containsKey("ulong"))
             testULong();
         if(!exclusionHash.containsKey("ulonglong"))
             testULonglong();
         if(!exclusionHash.containsKey("float"))
             testFloat();
         if(!exclusionHash.containsKey("double"))
             testDouble();
         if(!exclusionHash.containsKey("boolean"))
             testBoolean();
         if(!exclusionHash.containsKey("string"))
             testString();
         if(!exclusionHash.containsKey("wstring"))
             testWString();
         if(!exclusionHash.containsKey("stringArray"))
             testStringArray();
         if(!exclusionHash.containsKey("longArray"))
             testLongArray();
         if(!exclusionHash.containsKey("charArray"))
             testCharArray();
         if(!exclusionHash.containsKey("object"))
             testObject();
         if(!exclusionHash.containsKey("mixed"))
             testMixed();
    }

    private void testShort() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testShort");
        short[] shortVar = new short[1];

        shortVar[0]=java.lang.Short.MIN_VALUE;
        s.append(shortVar[0]+"\n");
        server.testShort(shortVar);
        s2.append(shortVar[0]+"\n");
        shortVar[0]=0;
        s.append(shortVar[0]+"\n");
        server.testShort(shortVar);
        s2.append(shortVar[0]+"\n");
        shortVar[0]=VarContainer.shortVar;
        s.append(shortVar[0]+"\n");
        server.testShort(shortVar);
        s2.append(shortVar[0]+"\n");
        shortVar[0]=java.lang.Short.MAX_VALUE;
        s.append(shortVar[0]+"\n");
        server.testShort(shortVar);
        s2.append(shortVar[0]+"\n");
        shortVar[0]=112;
        server.testShort(shortVar);
        printResult(s.toString(),"j2x.inout.client.short");
        printResult(s2.toString(),"j2x.inout.xclient.short");
    }

    private void testLong() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testLong");
        int[] intVar = new int[1];
        intVar[0]=java.lang.Integer.MIN_VALUE;
        s.append(intVar[0]+"\n");
        server.testLong(intVar);
        s2.append(intVar[0]+"\n");
        intVar[0]=0;
        s.append(intVar[0]+"\n");
        server.testLong(intVar);
        s2.append(intVar[0]+"\n");
        intVar[0]=1000;//VarContainer.intVar;
        s.append(intVar[0]+"\n");
        server.testLong(intVar);
        s2.append(intVar[0]+"\n");
        intVar[0]=java.lang.Integer.MAX_VALUE;
        s.append(intVar[0]+"\n");
        server.testLong(intVar);
        s2.append(intVar[0]+"\n");
        intVar[0]=112;
        server.testLong(intVar);
        printResult(s.toString(),"j2x.inout.client.long");
        printResult(s2.toString(),"j2x.inout.xclient.long");
    }

    private void testLonglong() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        
        System.err.println("server.testLonglong");
        long[] longVar = new long[1];

        longVar[0]=java.lang.Long.MIN_VALUE;
        s.append(longVar[0]+"\n");
        server.testLonglong(longVar);
        s2.append(longVar[0]+"\n");
        longVar[0]=0;
        s.append(longVar[0]+"\n");
        server.testLonglong(longVar);
        s2.append(longVar[0]+"\n");
        longVar[0]=VarContainer.longVar;
        s.append(longVar[0]+"\n");
        server.testLonglong(longVar);
        s2.append(longVar[0]+"\n");
        longVar[0]=java.lang.Long.MAX_VALUE;
        s.append(longVar[0]+"\n");
        server.testLonglong(longVar);
        s2.append(longVar[0]+"\n");
        longVar[0]=112;
        server.testLonglong(longVar);    
        printResult(s.toString(),"j2x.inout.client.longlong");
        printResult(s2.toString(),"j2x.inout.xclient.longlong");
    }

    private void testByte() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testByte");
        byte[] byteVar = new byte[1];
        
        byteVar[0]=java.lang.Byte.MIN_VALUE;
        s.append(byteVar[0]+"\n");
        server.testByte(byteVar);
        s2.append(byteVar[0]+"\n");
        byteVar[0]=0;
        s.append(byteVar[0]+"\n");
        server.testByte(byteVar);
        s2.append(byteVar[0]+"\n");
        byteVar[0]=VarContainer.unsignedByteVar;
        s.append(byteVar[0]+"\n");
        server.testByte(byteVar);
        s2.append(byteVar[0]+"\n");
        byteVar[0]=java.lang.Byte.MAX_VALUE;
        s.append(byteVar[0]+"\n");
        server.testByte(byteVar);
        s2.append(byteVar[0]+"\n");
        byteVar[0]=112;
        server.testByte(byteVar);
        printResult(s.toString(),"j2x.inout.client.octet");
        printResult(s2.toString(),"j2x.inout.xclient.octet");
    }
    
    
    private void testUShort() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testUShort");
        short[] ushortVar = new short[1];
        
        ushortVar[0]=0;
        s.append(ushortVar[0]+"\n");
        server.testUShort(ushortVar);
        s2.append(ushortVar[0]+"\n");
        ushortVar[0]=1000;//VarContainer.unsignedShortVar;
        s.append(ushortVar[0]+"\n");
        server.testUShort(ushortVar);
        s2.append(ushortVar[0]+"\n");
        ushortVar[0]=java.lang.Short.MAX_VALUE;
        s.append(ushortVar[0]+"\n");
        server.testUShort(ushortVar);
        s2.append(ushortVar[0]+"\n");
        ushortVar[0]=112;
        server.testUShort(ushortVar);
        printResult(s.toString(),"j2x.inout.client.ushort");
        printResult(s2.toString(),"j2x.inout.xclient.ushort");
    }

    private void testULong() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testUlong");
        int[] uintVar = new int[1];
        
        uintVar[0]=0;
        s.append(uintVar[0]+"\n");
        server.testULong(uintVar);
        s2.append(uintVar[0]+"\n");
        uintVar[0]=1000;//VarContainer.unsignedIntVar;
        s.append(uintVar[0]+"\n");
        server.testULong(uintVar);
        s2.append(uintVar[0]+"\n");
        uintVar[0]=java.lang.Integer.MAX_VALUE;
        s.append(uintVar[0]+"\n");
        server.testULong(uintVar);
        s2.append(uintVar[0]+"\n");
        uintVar[0]=112;
        server.testULong(uintVar);
        printResult(s.toString(),"j2x.inout.client.ulong");
        printResult(s2.toString(),"j2x.inout.xclient.ulong");
    }

    private void testULonglong() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testULonglong");
        long[] ulongVar = new long[1];
        
        ulongVar[0]=0;
        s.append(ulongVar[0]+"\n");
        server.testULonglong(ulongVar);
        s2.append(ulongVar[0]+"\n");
        ulongVar[0]=VarContainer.unsignedLongVar;
        s.append(ulongVar[0]+"\n");
        server.testULonglong(ulongVar);
        s2.append(ulongVar[0]+"\n");
        ulongVar[0]=java.lang.Long.MAX_VALUE;
        s.append(ulongVar[0]+"\n");
        server.testULonglong(ulongVar);
        s2.append(ulongVar[0]+"\n");
        ulongVar[0]=112;
        server.testULonglong(ulongVar);
        printResult(s.toString(),"j2x.inout.client.ulonglong");
        printResult(s2.toString(),"j2x.inout.xclient.ulonglong");
    }
    private void testFloat() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testFloat");
        float[] floatVar = new float[1];
        
        floatVar[0]=java.lang.Float.MIN_VALUE;
        s.append(floatVar[0]+"\n");
        server.testFloat(floatVar);
        s2.append(floatVar[0]+"\n");
        floatVar[0]=0;
        s.append(floatVar[0]+"\n");
        server.testFloat(floatVar);
        s2.append(floatVar[0]+"\n");
        floatVar[0]=1000;//VarContainer.floatVar;
        s.append(floatVar[0]+"\n");
        server.testFloat(floatVar);
        s2.append(floatVar[0]+"\n");
        floatVar[0]=java.lang.Float.MAX_VALUE;
        s.append(floatVar[0]+"\n");
        server.testFloat(floatVar);
        s2.append(floatVar[0]+"\n");
        floatVar[0]=112;
        server.testFloat(floatVar);
        printResult(s.toString(),"j2x.inout.client.float");
        printResult(s2.toString(),"j2x.inout.xclient.float");
    }
    private void testDouble() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testDouble");
        double[] doubleVar = new double[1];
        
        doubleVar[0]=java.lang.Double.MIN_VALUE;
        s.append(doubleVar[0]+"\n");
        server.testDouble(doubleVar);
        s2.append(doubleVar[0]+"\n");
        doubleVar[0]=0;
        s.append(doubleVar[0]+"\n");
        server.testDouble(doubleVar);
        s2.append(doubleVar[0]+"\n");
        doubleVar[0]=VarContainer.doubleVar;
        s.append(doubleVar[0]+"\n");
        server.testDouble(doubleVar);
        s2.append(doubleVar[0]+"\n");
        doubleVar[0]=java.lang.Double.MAX_VALUE;
        s.append(doubleVar[0]+"\n");
        server.testDouble(doubleVar);
        s2.append(doubleVar[0]+"\n");
        doubleVar[0]=112;
        server.testDouble(doubleVar);
        
        
        printResult(s.toString(),"j2x.inout.client.double");
        printResult(s2.toString(),"j2x.inout.xclient.double");
    }


    private void testBoolean() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testBoolean");
        boolean[] boolVar = new boolean[1];
        
        boolVar[0]=true;
        s.append(boolVar[0]+"\n");
        server.testBoolean(boolVar);
        s2.append(boolVar[0]+"\n");
        boolVar[0]=false;
        s.append(boolVar[0]+"\n");
        server.testBoolean(boolVar);
        s2.append(boolVar[0]+"\n");
        
        printResult(s.toString(),"j2x.inout.client.boolean");
        printResult(s2.toString(),"j2x.inout.xclient.boolean");
    }
    
    
    private void testChar() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testChar");
        char[] charVar = new char[1];
        
        charVar[0]='0';
        s.append(charVar[0]+"\n");
        server.testChar(charVar);
        s2.append(charVar[0]+"\n");
        charVar[0]='Z';
        s.append(charVar[0]+"\n");
        server.testChar(charVar);
        s2.append(charVar[0]+"\n");
        charVar[0]='x';
        server.testChar(charVar);
        
        printResult(s.toString(),"j2x.inout.client.char");
        printResult(s2.toString(),"j2x.inout.xclient.char");
    }


    private void testWChar() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testWChar");
        char[] wcharVar = new char[1];
        
        wcharVar[0]='0';
        s.append(wcharVar[0]+"\n");
        server.testWChar(wcharVar);
        s2.append(wcharVar[0]+"\n");
        wcharVar[0]='Z';
        s.append(wcharVar[0]+"\n");
        server.testWChar(wcharVar);
        s2.append(wcharVar[0]+"\n");
        //	 wcharVar[0]='x';
        //         server.testWChar(wcharVar);
        
        printResult(s.toString(),"j2x.inout.client.wchar");
        printResult(s2.toString(),"j2x.inout.xclient.wchar");
    }


    private void testString() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testString");
        String[] stringVar = new String[1];
        
        stringVar[0]="111";//VarContainer.charPVar;
        s.append(stringVar[0]+"\n");
        server.testString(stringVar);
        s2.append(stringVar[0]+"\n");
        stringVar[0]="";
        s.append(stringVar[0]+"\n");
        server.testString(stringVar);
        s2.append(stringVar[0]+"\n");
        stringVar[0]="NULL string";
        s.append(stringVar[0]+"\n");
        server.testString(stringVar);
        s2.append(stringVar[0]+"\n");
        //	 stringVar[0]="112";
        //         server.testString(stringVar);
        
        
        printResult(s.toString(),"j2x.inout.client.string");
        printResult(s2.toString(),"j2x.inout.xclient.string");
    }


    private void testWString() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testWString");
        String[] wstringVar = new String[1];
        
        wstringVar[0]="111";//VarContainer.unicharPVar;
        s.append(wstringVar[0]+"\n");
        server.testWString(wstringVar);
        s2.append(wstringVar[0]+"\n");
        wstringVar[0]="";
        s.append(wstringVar[0]+"\n");
        server.testWString(wstringVar);
        s2.append(wstringVar[0]+"\n");
        wstringVar[0]="NULL string";
        s.append(wstringVar[0]+"\n");
        server.testWString(wstringVar);
        s2.append(wstringVar[0]+"\n");
        //	 wstringVar[0]="112";
        //         server.testWString(wstringVar);
        printResult(s.toString(),"j2x.inout.client.wstring");
        printResult(s2.toString(),"j2x.inout.xclient.wstring");
    }
    

    private void testStringArray() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testStringArray");
        String[][] stringAVar = new String[1][3];

        stringAVar[0][0]="iddqd";
        stringAVar[0][1]="idkfa";
        stringAVar[0][2]="ctrl-q";
        int count=3;
        for(int i=0;i<count;i++)
            s.append(stringAVar[0][i]+"\n");
        server.testStringArray(count,stringAVar);
        for(int i=0;i<count;i++)
            s2.append(stringAVar[0][i]+"\n");
        
        
        printResult(s.toString(),"j2x.inout.client.stringArray");
        printResult(s2.toString(),"j2x.inout.xclient.stringArray");
    }


    private void testLongArray() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testLongArray");
        int[][] intAVar = new int[1][3];
        
        intAVar[0][0]=777;
        intAVar[0][1]=888;
        intAVar[0][2]=999;
        int count=3;
        for(int i=0;i<count;i++)
            s.append(intAVar[0][i]+"\n");
        server.testLongArray(count,intAVar);
        for(int i=0;i<count;i++)
            s2.append(intAVar[0][i]+"\n");
        
        printResult(s.toString(),"j2x.inout.client.longArray");
        printResult(s2.toString(),"j2x.inout.xclient.longArray");
    }

    private void testCharArray () {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testCharArray");
        char[][] charAVar = new char[1][3];
        
        charAVar[0][0]='k';
        charAVar[0][1]='f';
        charAVar[0][2]='a';
        int count=3;
        for(int i=0;i<count;i++)
            s.append(charAVar[0][i]+"\n");
        server.testCharArray(count,charAVar);
        for(int i=0;i<count;i++)
            s2.append(charAVar[0][i]+"\n");
        
        
        printResult(s.toString(),"j2x.inout.client.charArray");
        printResult(s2.toString(),"j2x.inout.xclient.charArray");
    }
    
    private void testMixed() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testMixed");
        
        short[] shortVar=new short[1];
        int[] intVar=new int[1];
        long[] longVar=new long[1];
        byte[] byteVar=new byte[1];
        short[] ushortVar=new short[1];
        int[] uintVar=new int[1];
        char[] charVar=new char[1];
        long[] ulongVar=new long[1];
        float[] floatVar=new float[1];
        double[] doubleVar=new double[1];
        int count=3;
        String[] stringVar=new String[1];
        boolean[] boolVar=new boolean[1];
        int[][] intAVar=new int[1][3];
        
        shortVar[0]=VarContainer.shortVar;
        intVar[0]=VarContainer.intVar;
        longVar[0]=VarContainer.longVar;
        byteVar[0]=VarContainer.unsignedByteVar;
        ushortVar[0]=VarContainer.unsignedShortVar;
        uintVar[0]=VarContainer.unsignedIntVar;
        ulongVar[0]=VarContainer.unsignedLongVar;
        floatVar[0]=VarContainer.floatVar;
        doubleVar[0]=VarContainer.doubleVar;
        boolVar[0]=VarContainer.booleanVar;
        charVar[0]=VarContainer.charVar;
        stringVar[0]=VarContainer.charPVar;
        intAVar[0][0]=333;
        intAVar[0][1]=444;
        intAVar[0][2]=555;
        
        s.append(boolVar[0]+"\n"+charVar[0]+"\n"+byteVar[0]+"\n"+
                 shortVar[0]+"\n"+ushortVar[0]+"\n"+intVar[0]+"\n"+
                 uintVar[0]+"\n"+longVar[0]+"\n"+ulongVar[0]+"\n"+
                 floatVar[0]+"\n"+doubleVar[0]+"\n"+stringVar[0]+"\n"+
                 intAVar[0][0]+"\n"+intAVar[0][1]+"\n"+intAVar[0][2]+"\n");
        
        server.testMixed(boolVar, charVar, byteVar, shortVar, ushortVar,
                         intVar, uintVar, longVar, ulongVar, floatVar, 
                         doubleVar, stringVar, count, intAVar);
        
        s2.append(boolVar[0]+"\n"+charVar[0]+"\n"+byteVar[0]+"\n"+
                  shortVar[0]+"\n"+ushortVar[0]+"\n"+intVar[0]+"\n"+
                  uintVar[0]+"\n"+longVar[0]+"\n"+ulongVar[0]+"\n"+
                  floatVar[0]+"\n"+doubleVar[0]+"\n"+stringVar[0]+"\n"+
                  intAVar[0][0]+"\n"+intAVar[0][1]+"\n"+intAVar[0][2]+"\n");
        
        printResult(s.toString(),"j2x.inout.client.mixed");
        printResult(s2.toString(),"j2x.inout.xclient.mixed");
    }

    private void testObject() {
        StringBuffer s = new StringBuffer();
        StringBuffer s2 = new StringBuffer();
        System.err.println("server.testObject");
        iJ2XINOUTServerTestComponent[] objectVar= new iJ2XINOUTServerTestComponent[1];
        objectVar[0] = server;
        
        printResult("!!!Right string!!!","j2x.inout.client.object"); //!!!Mat' Mat' 
        server.testObject(objectVar);
        objectVar[0].testObj2();	 
	}

    public Object queryInterface(IID iid) {
        System.out.println("DEbug:avm:J2XINOUTClientTestComponent::queryInterface iid="+iid);
        if ( iid.equals(nsISupports.IID)
             || iid.equals(iJ2XINOUTClientTestComponent.IID)||iid.equals(iClientTestComponent.IID)||iid.equals(iExclusionSupport.IID)) {
            return this;
        } else {
            return null;
        }
    }

    static CID J2XINOUTServerCID = new CID("4aa238d4-d655-40b7-9743-62a5b2e21a5c");
    static {
        try {

            try {
                System.err.println("J2XINOUTClientTestComponent Classloader is "    + Class.forName("J2XINOUTClientTestComponent").getClassLoader());
                Class c = Class.forName("S");
            }catch(Exception e) {
                System.err.println("J2XINOUTClientTestComponent Classloa BLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL");
            }
            System.out.println("J2XINOUTClientTestComponent - static block ");
            Class nsIComponentManagerClass = 
                Class.forName("org.mozilla.xpcom.nsIComponentManager"); 
            InterfaceRegistry.register(nsIComponentManagerClass);
            Class proxyHandlerClass = 
                Class.forName("org.mozilla.xpcom.ProxyHandler");       
            InterfaceRegistry.register(proxyHandlerClass);
            Class nsIFileClass = 
                Class.forName("org.mozilla.xpcom.nsIFile");       
            InterfaceRegistry.register(nsIFileClass);
            Class nsIFactoryClass = 
                Class.forName("org.mozilla.xpcom.nsIFactory");       
            InterfaceRegistry.register(nsIFactoryClass);
            Class nsIEnumeratorClass = 
                Class.forName("org.mozilla.xpcom.nsIEnumerator");
            InterfaceRegistry.register(nsIEnumeratorClass);
            System.out.println("DE bug:avm:before registeringiJ2XINOUTServerTestComponent ");
            Class iJ2XINOUTServerTestComponentClass = 
                Class.forName("org.mozilla.xpcom.iJ2XINOUTServerTestComponent");
            InterfaceRegistry.register(iJ2XINOUTServerTestComponentClass);
        } catch (Exception e) {
            System.err.println("#####################################################################");
            System.err.println("####################EXCEPTION during interface initialization############");
            System.err.println(e);
        } catch (Error e) {
            System.err.println("#####################################################################");
            System.err.println("####################Error during interface initialization############");
            System.err.println(e);
        }
    }

}
