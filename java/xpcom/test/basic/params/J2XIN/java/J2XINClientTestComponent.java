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

public class J2XINClientTestComponent implements iJ2XINClientTestComponent, iClientTestComponent, iJClientTestComponent, iExclusionSupport {
    private iJ2XINServerTestComponent server = null;
    private VarContainer varContainer = null;
    private String testLocation = null;
    private String logLocation = null;
    private Hashtable exclusionHash = new Hashtable();
    private StringBuffer buf, s;

    public J2XINClientTestComponent() {
        System.out.println("DEbug:avm:J2XINClientTestComponent constructor");
        varContainer = new VarContainer();
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
        System.out.println("DEbug:avm:J2XINClientTestComponent:exclude");
        for(int i=0;i<count;i++) {
            exclusionHash.put((Object)exclusionList[i],new Object());
        }
    }

    /* void Initialize (in string serverProgID); */
    public void initialize(String serverProgID) {
        System.out.println("DEbug:avm:J2XINClientTestComponent:initialize");
        //Really code from tHack should be here!!
    }
    public void tHack(nsIComponentManager cm, String serverProgID) {
        System.out.println("DEbug:avm:J2XINClientTestComponent:tHack");
        nsIFactory factory = null;
        
        if(cm == null) {
            System.out.println("DEbug:avm:ComponentManager is NULL!!!!!");
            return;
        }
        factory = cm.findFactory(J2XINServerCID);
        if(factory == null) {
            System.out.println("DEbug:avm:Factory is NULL!!!!!");
            return;
        }
//System.err.println("IID="+iJ2XINServerTestComponent.IID);
        Object res = factory.createInstance(null, iJ2XINServerTestComponent.IID);
//      Object res = factory.createInstance(null, new IID("3b0e2d20-9852-11d4-aa22-00a024a8bbac"));
        if(res == null) {
            System.out.println("DEbug:avm:Instance is NULL!!!!!");
            return;
        }
        server = (iJ2XINServerTestComponent)res;
        if(server == null) {
             System.err.println("Create instance failed!! Server is NULLLLLLLLLLLLLLLLLLL");
             return;
        }
        String[] s2 = new String[1];
        String[] s1 = new String[1];
        server.getTestLocation(s2,s1);
        testLocation = s2[0];
        logLocation = s1[0];
        
    } 

    /* void Execute (); */
    public void execute() {
         System.out.println("DEbug:avm:J2XINClientTestComponent:execute");
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
         if(!exclusionHash.containsKey("iid"))
             testIID();
         if(!exclusionHash.containsKey("cid"))
             testCID();

    }


    private void testShort() {
         StringBuffer buf = new StringBuffer();
         System.err.println("server.testShort");
         short shortVal = varContainer.getNextShort();
         while(shortVal != VarContainer.endOfData) {
             buf.append(shortVal + "\n");
             server.testShort(shortVal);
             shortVal = varContainer.getNextShort();
         }
         printResult(buf.toString(),"j2x.in.client.short");
         server.flush("short");
    }

    private void testLong() {         
         System.err.println("server.testLong");
         buf = new StringBuffer("");
         int intVal = varContainer.getNextInt();
         while(intVal != VarContainer.endOfData) {
             buf.append(intVal + "\n");
             server.testLong(intVal);
             intVal = varContainer.getNextInt();
         }
         printResult(buf.toString(),"j2x.in.client.long");
         server.flush("long");
    }

    private void testLonglong() {
         System.err.println("server.testLonglong");
         buf = new StringBuffer("");
         long longVal = varContainer.getNextLong();
         while(longVal != VarContainer.endOfData) {
             buf.append(longVal + "\n");
             server.testLonglong(longVal);
             longVal = varContainer.getNextLong();
         }
         printResult(buf.toString(),"j2x.in.client.longlong");
         server.flush("longlong");
    }

    private void testByte() {
         buf = new StringBuffer("");
         byte byteVal = varContainer.getNextByte();
         while(byteVal != VarContainer.endOfData) {
             buf.append(byteVal + "\n");
             server.testByte(byteVal);
             byteVal = varContainer.getNextByte();
         }
         printResult(buf.toString(),"j2x.in.client.octet");
         server.flush("octet");
    }

    private void testUShort() {
         buf = new StringBuffer("");
         short ushortVal = varContainer.getNextUshort();
         while(ushortVal != VarContainer.endOfData) {
             buf.append(ushortVal + "\n");
             server.testUShort(ushortVal);
             ushortVal = varContainer.getNextUshort();
         }
         printResult(buf.toString(),"j2x.in.client.ushort");
         server.flush("ushort");
    }
     
    private void testULong() {
         buf = new StringBuffer("");
         int uintVal = varContainer.getNextUint();
         while(uintVal != VarContainer.endOfData) {
             buf.append(uintVal + "\n");
             server.testULong(uintVal);
             uintVal = varContainer.getNextUint();
         }
         printResult(buf.toString(),"j2x.in.client.ulong");
         server.flush("ulong");
    }

    private void testULonglong() {
         buf = new StringBuffer("");
         long ulongVal = varContainer.getNextUlong();
         while(ulongVal != VarContainer.endOfData) {
             buf.append(ulongVal + "\n");
             server.testULonglong(ulongVal);
             ulongVal = varContainer.getNextUlong();
         }
         printResult(buf.toString(),"j2x.in.client.ulonglong");
         server.flush("ulonglong");
    }

    private void testFloat() {
         buf = new StringBuffer("");
         float floatVal = varContainer.getNextFloat();
         while(floatVal != VarContainer.endOfData) {
             buf.append(floatVal + "\n");
             server.testFloat(floatVal);
             floatVal = varContainer.getNextFloat();
         }
         printResult(buf.toString(),"j2x.in.client.float");
         server.flush("float");
    }

    private void testDouble() {
         buf = new StringBuffer("");
         double doubleVal = varContainer.getNextDouble();
         while(doubleVal != VarContainer.endOfData) {
             buf.append(doubleVal + "\n");
             server.testDouble(doubleVal);
             doubleVal = varContainer.getNextDouble();
         }
         printResult(buf.toString(),"j2x.in.client.double");
         server.flush("double");
    }

    private void testBoolean() {
         server.testBoolean(true);
         server.testBoolean(false);
         printResult("true\nfalse","j2x.in.client.boolean");
         server.flush("boolean");
    }

//    private void testChar() {
//         server.testChar('S');
//         server.testWChar(VarContainer.charVar);
//         printResult("S","j2x.in.client.char");
//         server.flush("char");
//      }

    private void testString() {
         printResult(VarContainer.charPVar,"j2x.in.client.string");
         server.testString(VarContainer.charPVar);
         server.flush("string");
    }

    private void testWString() {
         printResult(VarContainer.unicharPVar,"j2x.in.client.wstring");
         server.testWString(VarContainer.unicharPVar);
         server.flush("wstring");
    }

    private void testStringArray() {
         String[] valueArray = {"fist string", "second string", "some string","Sample string 1","S","String SSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS"};
         int count = valueArray.length;
         s = new StringBuffer();
         for(int i=0;i<count;i++) {
             s.append(valueArray[i]+"\n");
         }
         printResult(s.toString(),"j2x.in.client.stringArray");
         server.testStringArray(count, valueArray);
    }

    private void testLongArray() {
         int[] intArray = {1,2,201,-6,86,-10000};
         s = new StringBuffer();
         for(int i=0;i<intArray.length;i++) {
             s.append(intArray[i]+"\n");
         }
         printResult(s.toString(),"j2x.in.client.longArray");
         server.testLongArray(intArray.length, intArray);
    }
     
    private void testCharArray() {
         char[] charArray = {'A','B','c','L','ï','u','Ô','p'};
         s = new StringBuffer();
         for(int i=0;i<charArray.length;i++) {
             s.append(charArray[i]+"\n");
         }
         printResult(s.toString(),"j2x.in.client.charArray");
         server.testCharArray(charArray.length, charArray);
    }

    private void testMixed() {
         boolean bBool = true;
         char cChar = '0';
         byte nByte = VarContainer.byteMid;
         short nShort = VarContainer.shortMid;
         short nUShort = VarContainer.ushortMid;
         int nLong = VarContainer.intMid;
         int nULong = VarContainer.uintMid;
         long nHyper = VarContainer.longMid;
         long nUHyper = VarContainer.ulongMid;
         float fFloat = VarContainer.floatMid;
         double fDouble = VarContainer.doubleMid;
         String aString = "Mixed string";
         int[] intArray = {1,2,201,-6,86,-10000};

         s = new StringBuffer();
         s.append(bBool + "\n");
         s.append(cChar + "\n");
         s.append(nByte + "\n");
         s.append(nShort + "\n");
         s.append(nUShort + "\n");
         s.append(nLong + "\n");
         s.append(nULong + "\n");
         s.append(nHyper + "\n");
         s.append(nUHyper + "\n");
         s.append(fFloat + "\n");
         s.append(fDouble + "\n");
         s.append(aString + "\n");
         for(int j=0;j<intArray.length;j++)  
             s.append(intArray[j] + "\n");

         printResult(s.toString(),"j2x.in.client.mixed");
         server.testMixed(bBool, nByte, nShort, nUShort,nLong,nULong, nHyper, nUHyper, fFloat,fDouble, aString, intArray.length, intArray);
    }
     
    private void testObject() {
         printResult(server.getTestObjectString(),"j2x.in.client.object");
         server.testObject(server);
    }

    private void testIID() {
         buf = new StringBuffer("");
 	 IID iid=new IID("cc7480e0-3a37-11d5-b653-005004552ed1");
         buf.append(iid + "\n");

         server.testIID(iid);
         printResult(buf.toString(),"j2x.in.client.iid");
         server.flush("iid");
    }

    private void testCID() {
         buf = new StringBuffer("");
 	 CID cid=new CID("cc7480e0-3a37-11d5-b653-005004552ed1");
         buf.append(cid + "\n");

         server.testCID(cid);
         printResult(buf.toString(),"j2x.in.client.cid");
         server.flush("cid");
    }



    public Object queryInterface(IID iid) {
        System.out.println("DEbug:avm:J2XINClientTestComponent::queryInterface iid="+iid);
        if ( iid.equals(nsISupports.IID)
             || iid.equals(iJ2XINClientTestComponent.IID)||iid.equals(iClientTestComponent.IID)
		||iid.equals(iExclusionSupport.IID))  {
            return this;
        } else {
            return null;
        }
    }

    static CID J2XINServerCID = new CID("1ddc5b10-9852-11d4-aa22-00a024a8bbac");
//    static IID iJ2XINClientTestComponentIID = new IID(iJ2XINClientTestComponent.IID);
//    static IID iClientTestComponentIID = new IID(iClientTestComponent.IID);
//    static IID iJ2XINServerIID = new IID(iJ2XINServerTestComponent.IID);
//    static IID nsISupportsIID = new IID(nsISupports.IID);
//  static IID nsIComponentManagerIID = new IID(nsIComponentManager.IID);
//    static IID iExclusionSupportIID = new IID(iExclusionSupport.IID);
//    */
    static {
      try {

          System.out.println("J2XINClientTestComponent - static block ");
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
          System.out.println("DE bug:avm:before registeringiJ2XINServerTestComponent ");
          Class iJ2XINServerTestComponentClass = 
              Class.forName("org.mozilla.xpcom.iJ2XINServerTestComponent");
          InterfaceRegistry.register(iJ2XINServerTestComponentClass);
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

