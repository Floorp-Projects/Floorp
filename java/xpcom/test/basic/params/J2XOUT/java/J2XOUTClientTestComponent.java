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

public class J2XOUTClientTestComponent implements iJ2XOUTClientTestComponent, iClientTestComponent, iJClientTestComponent, iExclusionSupport {
    private iJ2XOUTServerTestComponent server = null;
    private String testLocation = null;
    private String logLocation = null;
    private byte endOfData = 112;
    private Hashtable exclusionHash = new Hashtable();
    
    public J2XOUTClientTestComponent() {
        System.out.println("DEbug:avm:J2XOUTClientTestComponent constructor");
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
    public void exclude(int count, String[] exclusionList) {
        System.out.println("DEbug:avm:J2XOUTClientTestComponent:exclude");
        for(int i=0;i<count;i++) {
            exclusionHash.put((Object)exclusionList[i],new Object());
        }
    }
    public void initialize(String serverProgID) {

        System.out.println("DEbug:avm:J2XOUTClientTestComponent:initialize");
        //Really code from tHack should be here!!
    }
    public void tHack(nsIComponentManager cm, String serverProgID) {
        System.out.println("DEbug:avm:J2XOUTClientTestComponent:tHack");
        nsIFactory factory = null;
        
        if(cm == null) {
            System.out.println("DEbug:avm:ComponentManager is NULL!!!!!");
            return;
        }
        factory = cm.findFactory(J2XOUTServerCID);
        if(factory == null) {
            System.out.println("DEbug:avm:Factory is NULL!!!!!");
            return;
        }
        Object res = factory.createInstance(null, iJ2XOUTServerTestComponent.IID);
        if(res == null) {
            System.out.println("DEbug:avm:Instance is NULL!!!!!");
            return;
        }
        server = (iJ2XOUTServerTestComponent)res;
        if(server == null) {
             System.err.println("Create instance failed!! Server is NULLLLLLLLLLLLLLLLLLL");
             return;
        }
        String[] s = new String[1];
        String[] s1 = new String[1];
        server.getTestLocation(s,s1);
        testLocation = s[0];
        logLocation = s1[0];
        
    } 

    /* void Execute (); */
    public void execute() {
         System.out.println("DEbug:avm:J2XOUTClientTestComponent:execute");
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
             testUshort();
         if(!exclusionHash.containsKey("ulong"))
             testsUlong();
         if(!exclusionHash.containsKey("ulonglong"))
             testsUlonglong();
         if(!exclusionHash.containsKey("float"))
             testsFloat();
         if(!exclusionHash.containsKey("double"))
             testDouble();
         if(!exclusionHash.containsKey("boolean"))
             testBoolean();
         if(!exclusionHash.containsKey("string"))
             testString();
         if(!exclusionHash.containsKey("wstring"))
             testsWstring();
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

    /*
        
    server.testMixed(boolean bBool, char cChar, byte nByte, short nShort, short nUShort, int nLong, int nULong, long nHyper, long nUHyper, float fFloat, double fDouble, String aString, int count, int longArray);

   */      

 

private void testShort() {
    StringBuffer s = new StringBuffer();
    System.err.println("server.testShort");
    short[] shortVar = new short[1];
    server.testShort(shortVar);
    while(shortVar[0] != endOfData) {
        s.append(shortVar[0]+"\n");
        server.testShort(shortVar);
    }
    printResult(s.toString(),"j2x.out.client.short");
    server.flush("short");
}
private void testLong() {
    StringBuffer s = new StringBuffer();
    int[] intVar = new int[1];
    System.err.println("server.testLong");
    server.testLong(intVar);
    while(intVar[0] != endOfData) {
        s.append(intVar[0]+"\n");
        server.testLong(intVar);
    }
    printResult(s.toString(),"j2x.out.client.long");
    server.flush("long");
}
private void testLonglong() {
    StringBuffer s = new StringBuffer();
    long[] longVar = new long[1];
    System.err.println("server.testLonglong");
    server.testLonglong(longVar);
    while(longVar[0] != endOfData) {
        s.append(longVar[0]+"\n");
        server.testLonglong(longVar);
    }
    printResult(s.toString(),"j2x.out.client.longlong");
    server.flush("longlong");
}
private void testByte() {
    StringBuffer s = new StringBuffer();
    byte [] byteVar = new byte[1];
    System.err.println("server.testByte");
    server.testByte(byteVar);
    while(byteVar[0] != endOfData) {
        s.append(byteVar[0]+"\n");
        server.testByte(byteVar);
    }
    printResult(s.toString(),"j2x.out.client.octet");
    server.flush("octet");
}
private void testUshort() {
    StringBuffer s = new StringBuffer();
    short [] ushortVar = new short[1];
    System.err.println("server.testUshort");
    server.testUShort(ushortVar);
    while(ushortVar[0] != endOfData) {
        s.append(ushortVar[0]+"\n");
        server.testUShort(ushortVar);
    }
    printResult(s.toString(),"j2x.out.client.ushort");
    server.flush("ushort");
}
private void testsUlong() {
    StringBuffer s = new StringBuffer();
    int [] uintVar = new int[1];
    System.err.println("server.testUlong");
    server.testULong(uintVar);
    while(uintVar[0] != endOfData) {
             s.append(uintVar[0]+"\n");
             server.testULong(uintVar);
    }
    printResult(s.toString(),"j2x.out.client.ulong");
    server.flush("ulong");
}     


private void testsUlonglong() {
    StringBuffer s = new StringBuffer();
    long [] ulongVar = new long[1];
    System.err.println("server.testUlonglong");
    server.testULonglong(ulongVar);
    while(ulongVar[0] != endOfData) {
        s.append(ulongVar[0]+"\n");
        server.testULonglong(ulongVar);
    }
    printResult(s.toString(),"j2x.out.client.ulonglong");
    server.flush("ulonglong");
}
private void testsFloat() {
    StringBuffer s = new StringBuffer();
    float [] floatVar = new float[1];
    System.err.println("server.testFloat");
    server.testFloat(floatVar);
    while(floatVar[0] != endOfData) {
        s.append(floatVar[0]+"\n");
        server.testFloat(floatVar);
    }
    printResult(s.toString(),"j2x.out.client.float");
    server.flush("float");
}
private void testDouble() {
    StringBuffer s = new StringBuffer();
    double [] doubleVar = new double[1];
    System.err.println("server.testDouble");
    server.testDouble(doubleVar);
    while(doubleVar[0] != endOfData) {
        s.append(doubleVar[0]+"\n");
        server.testDouble(doubleVar);
    }
    printResult(s.toString(),"j2x.out.client.double");
    server.flush("double");
}
private void testBoolean() {
    StringBuffer s = new StringBuffer();
    boolean [] booleanVar = new boolean[1];
    System.err.println("server.testBoolean");
    server.testBoolean(booleanVar);
    s.append(booleanVar[0]+"\n");
    server.testBoolean(booleanVar);
    s.append(booleanVar[0]+"\n");
    printResult(s.toString(),"j2x.out.client.boolean");
    server.flush("boolean");
}
       
         /*  
         //server.testChar(charVar);
         //server.testWChar(charVar);
         */
private void testString() {
    StringBuffer s = new StringBuffer();
    String[] stringVar = new String[1];
    server.testString(stringVar);
    s.append(stringVar[0]+"\n");
    printResult(s.toString(),"j2x.out.client.string");
    server.flush("string");
}
private void testsWstring() {    
    StringBuffer s = new StringBuffer();
    String[] stringVar = new String[1];
    server.testWString(stringVar);
    s.append(stringVar[0]+"\n");
    printResult(s.toString(),"j2x.out.client.wstring");
    server.flush("wstring");
}

private void testStringArray() {
    String[][] stringArray = new String[1][4];
    int count = 4;
    StringBuffer s = new StringBuffer();
    server.testStringArray(count, stringArray);
    System.out.println("IIP");    
    for(int i=0;i<count;i++) {
        s.append(stringArray[0][i]+"\n");
    }
    printResult(s.toString(),"j2x.out.client.stringArray");
}
private void testLongArray() {
    StringBuffer s = new StringBuffer();
    int[][] intArray = new int[1][100];
    int count = 100;
    server.testLongArray(count, intArray);
    for(int i=0;i<count;i++) {
        s.append(intArray[0][i]+"\n");
    }
    printResult(s.toString(),"j2x.out.client.longArray");
         
}
private void testCharArray() {
    char[][] charArray = new char[1][100];
    int count = 100;
    StringBuffer s = new StringBuffer();
    server.testCharArray(count, charArray);
    for(int i=0;i<count;i++) {
        s.append(charArray[0][i]+"\n");
    }
    printResult(s.toString(),"j2x.out.client.charArray");
}

    private void testMixed() {
        boolean[] bBool = new boolean[1];
        char[] cChar = new char[1]; //Skip this value due to JDK1.3 bug
        byte[] nByte = new byte[1];
        short[] nShort = new short[1];
        short[] nUShort = new short[1];
        int[] nLong = new int[1];
        int[] nULong = new int[1];
        long[] nHyper = new long[1];
        long[] nUHyper = new long[1];
        float[] fFloat = new float[1];
        double[] fDouble = new double[1];
        String[] aString = new String[1];
        int count = 5;
        int[][] longArray = new int[1][count];
        server.testMixed(bBool, nByte, nShort, nUShort, nLong, nULong, nHyper, nUHyper, fFloat, fDouble, aString, count, longArray);
        StringBuffer s = new StringBuffer();
		s.append(bBool[0]+"\n");
        s.append('A'+"\n");//Append zerro instead of skipped char
		s.append(nByte[0]+"\n"); 
		s.append(nShort[0]+"\n"); 
		s.append(nUShort[0]+"\n"); 
		s.append(nLong[0]+"\n"); 
		s.append(nULong[0]+"\n"); 
		s.append(nHyper[0]+"\n"); 
		s.append(nUHyper[0]+"\n"); 
		s.append(fFloat[0]+"\n"); 
		s.append(fDouble[0]+"\n"); 
		s.append(aString[0]+"\n"); 
		//s.append(count+"\n");
        for(int i=0;i<count;i++) {
            s.append(longArray[0][i]+"\n");
        }
        printResult(s.toString(),"j2x.out.client.mixed");
        //flush is'n required here !!!
    }


private void testObject() {
    iJ2XOUTServerTestComponent[] iServer = new iJ2XOUTServerTestComponent[1];
    server.testObject(iServer);
    printResult(iServer[0].getTestObjectString(),"j2x.out.client.object");
}     

    public Object queryInterface(IID iid) {
        System.out.println("DEbug:avm:J2XOUTClientTestComponent::queryInterface iid="+iid);
        if ( iid.equals(nsISupports.IID)
             || iid.equals(iJ2XOUTClientTestComponent.IID)||iid.equals(iClientTestComponent.IID)||iid.equals(iExclusionSupport.IID)) {
            return this;
        } else {
            return null;
        }
    }

    static CID J2XOUTServerCID = new CID("b1dcff02-0348-44ca-b662-29f5e375be27");
    static {
      try {

           try {
              System.err.println("2XINClientTestComponent Classloader is "    + Class.forName("J2XOUTClientTestComponent").getClassLoader());
              Class c = Class.forName("S");
            }catch(Exception e) {
                System.err.println("INClientTestComponent Classloa BLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL");
            }
          System.out.println("J2XOUTClientTestComponent - static block ");
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
          System.out.println("DE bug:avm:before registeringiJ2XOUTServerTestComponent ");
          Class iJ2XOUTServerTestComponentClass = 
              Class.forName("org.mozilla.xpcom.iJ2XOUTServerTestComponent");
          InterfaceRegistry.register(iJ2XOUTServerTestComponentClass);
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








