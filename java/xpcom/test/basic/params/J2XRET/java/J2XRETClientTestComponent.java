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

public class J2XRETClientTestComponent implements iJ2XRETClientTestComponent, iClientTestComponent, iJClientTestComponent, iExclusionSupport {
    private iJ2XRETServerTestComponent server = null;
    private String testLocation = null;
    private String logLocation = null;
    private String[] location1, location2;
    private byte endOfData = 112;
    private Hashtable exclusionHash = new Hashtable();
    private StringBuffer s;

    public J2XRETClientTestComponent() {
        System.out.println("DEbug:avm:J2XRETClientTestComponent constructor");
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
        System.out.println("DEbug:avm:J2XRETClientTestComponent:exclude");
        for(int i=0;i<count;i++) {
            exclusionHash.put((Object)exclusionList[i],new Object());
        }
    }

    public void initialize(String serverProgID) {

        System.out.println("DEbug:avm:J2XRETClientTestComponent:initialize");
        //Really code from tHack should be here!!
    }
    public void tHack(nsIComponentManager cm, String serverProgID) {
        System.out.println("DEbug:avm:J2XRETClientTestComponent:tHack--");
        nsIFactory factory = null;

        System.out.println("CM="+cm);        
        if(cm == null) {
            System.out.println("DEbug:avm:ComponentManager is NULL!!!!!");
            return;
        }
        factory = cm.findFactory(J2XRETServerCID);
        System.out.println("FAC="+factory);
        if(factory == null) {
            System.out.println("DEbug:avm:Factory is NULL!!!!!");
            return;
        }
        Object res = factory.createInstance(null, iJ2XRETServerTestComponent.IID);
        System.out.println("RES="+res);
        System.out.println("IID="+iJ2XRETServerTestComponent.IID);
        if(res == null) {
            System.out.println("DEbug:avm:Instance is NULL!!!!!");
            return;
        }
        server = (iJ2XRETServerTestComponent)res;
        System.out.println("SER="+server);
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
         System.out.println("DEbug:avm:J2XRETClientTestComponent:execute");

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

    }

    private void testShort() {
         s = new StringBuffer();
         short shortVar = 8;
         shortVar=server.testShort();
         while(shortVar != endOfData) {
             s.append(shortVar+"\n");
             shortVar=server.testShort();
         }
         printResult(s.toString(),"j2x.ret.client.short");
    }

    private void testLong() {
         s = new StringBuffer();
         int intVar = 8;
         intVar=server.testLong();
         while(intVar != endOfData) {
             s.append(intVar+"\n");
             intVar=server.testLong();
         }
         printResult(s.toString(),"j2x.ret.client.long");
    }

    private void testLonglong() {
         s = new StringBuffer();
         long longVar = 8;
         longVar=server.testLonglong();
         while(longVar != endOfData) {
             s.append(longVar+"\n");
             longVar=server.testLonglong();
         }
         printResult(s.toString(),"j2x.ret.client.longlong");
    }

    private void testByte() {
         s = new StringBuffer();
         byte byteVar = 8;
         byteVar=server.testByte();
         while(byteVar != endOfData) {
             s.append(byteVar+"\n");
             byteVar=server.testByte();
         }
         printResult(s.toString(),"j2x.ret.client.octet");
    }

    private void testUShort() {
         s = new StringBuffer();
         short ushortVar = 8;
         ushortVar=server.testUShort();
         while(ushortVar != endOfData) {
             s.append(ushortVar+"\n");
             ushortVar=server.testUShort();
         }
         printResult(s.toString(),"j2x.ret.client.ushort");
    }

    private void testULong() {
         s = new StringBuffer();
         int uintVar = 8;
         uintVar=server.testULong();
         while(uintVar != endOfData) {
             s.append(uintVar+"\n");
             uintVar=server.testULong();
         }
         printResult(s.toString(),"j2x.ret.client.ulong");
    }

    private void testULonglong() {
         s = new StringBuffer();
         long ulongVar = 8;
         ulongVar=server.testULonglong();
         while(ulongVar != endOfData) {
             s.append(ulongVar+"\n");
             ulongVar=server.testULonglong();
         }
         printResult(s.toString(),"j2x.ret.client.ulonglong");
    }

    private void testFloat() {
         s = new StringBuffer();
         float floatVar = 8;
         floatVar=server.testFloat();
         while(floatVar != endOfData) {
             s.append(floatVar+"\n");
             floatVar=server.testFloat();
         }
         printResult(s.toString(),"j2x.ret.client.float");
    }

    private void testDouble() {
         s = new StringBuffer();
         double doubleVar = 8;
         doubleVar=server.testDouble();
         while(doubleVar != endOfData) {
             s.append(doubleVar+"\n");
             doubleVar=server.testDouble();
         }
         printResult(s.toString(),"j2x.ret.client.double");
    }

    private void testBoolean() {
         s = new StringBuffer();
         boolean booleanVar = false;
         booleanVar=server.testBoolean();
         s.append(booleanVar+"\n");
         booleanVar=server.testBoolean();
         s.append(booleanVar+"\n");
         printResult(s.toString(),"j2x.ret.client.boolean");
         server.testBoolean();
    }

    private void testString() {
         s = new StringBuffer();
         String stringVar ="8";
         stringVar=server.testString();
	while(!stringVar.equals("112")) {
         s.append(stringVar+"\n");
         stringVar=server.testString();
        }
         s.append(stringVar+"\n");
         printResult(s.toString(),"j2x.ret.client.string");
         server.testString();
    }

    private void testWString() {
         s = new StringBuffer();
         String wstringVar ="abcdefg";
         wstringVar=server.testWString();
         s.append(wstringVar+"\n");
         wstringVar=server.testWString();
         s.append(wstringVar+"\n");
         wstringVar=server.testWString();
         s.append(wstringVar+"\n");
         printResult(s.toString(),"j2x.ret.client.wstring");
    }

    private void testLongArray() {
         s = new StringBuffer();
         int[] intAVar = {0,0,0};
         int count=3;
         intAVar=server.testLongArray(count);
      	 for(int i=0;i<count;i++)
           s.append(intAVar[i]+"\n");
         printResult(s.toString(),"j2x.ret.client.longArray");
    }

    private void testCharArray() {
         s = new StringBuffer();
         char[] charAVar = {'0','0','0'};
         int count=3;
         charAVar=server.testCharArray(count);
      	 for(int i=0;i<count;i++)
           s.append(charAVar[i]+"\n");
         printResult(s.toString(),"j2x.ret.client.charArray");
    }

    private void testStringArray() {
         s = new StringBuffer();
         String[] stringAVar = {"0","0","0"};
         int count=3;
         stringAVar=server.testStringArray(count);
      	 for(int i=0;i<count;i++)
           s.append(stringAVar[i]+"\n");
         printResult(s.toString(),"j2x.ret.client.stringArray");
    }

    private void testObject() {
         s = new StringBuffer();
         iJ2XRETServerTestComponent obj=null;
         try {
	      obj=server.testObject();
	      obj.testObj();
	 }catch(Exception e) {
         printResult(e.toString(),"j2x.ret.client.object");
	 }
         printResult("!!!Right string!!!","j2x.ret.client.object");
    }

    private void testWChar() {
         s = new StringBuffer();
         char wcharVar = '8';
         wcharVar=server.testWChar();
         while(wcharVar != endOfData) {
             s.append(wcharVar+"\n");
             wcharVar=server.testWChar();
         }
         printResult(s.toString(),"j2x.ret.client.wchar");
    }

    private void testChar() {
         s = new StringBuffer();
         char charVar = '8';
         charVar=server.testChar();
         while(charVar != endOfData) {
             s.append(charVar+"\n");
             charVar=server.testChar();
         }
         printResult(s.toString(),"j2x.ret.client.char");
     }

    public Object queryInterface(IID iid) {
        System.out.println("DEbug:avm:J2XRETClientTestComponent::queryInterface iid="+iid);
        if ( iid.equals(nsISupports.IID)
             || iid.equals(iJ2XRETClientTestComponent.IID)
		||iid.equals(iClientTestComponent.IID)
		||iid.equals(iExclusionSupport.IID)) {
            return this;
        } else {
            return null;
        }
    }

    static CID J2XRETServerCID = new CID("1c274ca0-a414-11d4-9d3d-00a024a8bb88");
    static {
      try {
          System.out.println("J2XRETClientTestComponent - static block ");
          Class iJ2XRETServerTestComponentClass = 
              Class.forName("org.mozilla.xpcom.iJ2XRETServerTestComponent");
          InterfaceRegistry.register(iJ2XRETServerTestComponentClass);
          Class nsIComponentManagerClass = 
              Class.forName("org.mozilla.xpcom.nsIComponentManager"); 
          InterfaceRegistry.register(nsIComponentManagerClass);
           Class proxyHandlerClass = 
              Class.forName("org.mozilla.xpcom.ProxyFactory");       
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
          System.out.println("DE bug:avm:before registeringiJ2XRETServerTestComponent ");
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
