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


public class MThreadComponent1 implements iMThreadComponent1, iMThreadComponent {
    private iMThreadContext context = null;
    private nsIComponentManager com=null;
    private int thr=0;
    private int stg=0;

    public MThreadComponent1() {
        System.out.println("DEbug:avm:MThreadComponent1 constructor");
    }

    public void initialize(iMThreadContext context) {
        System.out.println("DEbug:avm:MThreadComponent1:initialize Thread is " + Thread.currentThread().getName());
    }

    public void tHack(nsIComponentManager cm) {
        nsIFactory factory = null;
	System.out.println("DEbug:avm:tHack");
        if(cm == null) {
            System.err.println("DEbug:avm:tHack:ComponentManager is NULL!!!!!");
            return;
        }
	com=cm;
        factory = cm.findFactory(iMThreadContextCID);

        if(factory == null) {
            System.err.println("DEbug:avm:tHack:Factory is NULL!!!!!");
            return;
        }

        Object res = factory.createInstance(null, iMThreadContext.IID);
        if(res == null) {
            System.err.println("DEbug:avm:tHack:Instance is NULL!!!!!");
            return;
        }
        context = (iMThreadContext)res;
        if(context == null) {
             System.err.println("Create instance failed!! Server is NULLLLLLLLLLLLLLLLLLL");
             return;
        }
    }


    public iMThreadComponent loadComponent(CID classID) {
        System.out.println("DEbug:avm:MThreadComponent1:loadComponent");
        nsIComponentManager cm = context.getComponentManager();

        nsIFactory factory = cm.findFactory(classID);
        if(factory == null) {
            System.err.println("DEbug:avm:loadComponent:Factory is NULL!!!!!");
            return null;
        }

        iMThreadComponent component = (iMThreadComponent)factory.createInstance(null, iMThreadComponent.IID);
        return component;
    }


    public void execute(int thread, int stage) {
        System.out.println("DEbug:avm:MThreadComponent1:execute.");
        String cThreadName = Thread.currentThread().getName();

       try{
           DataOutputStream f=new DataOutputStream(new FileOutputStream(context.getResFile(),true)); 
           f.writeBytes((new Integer(thread)).toString()+","+(new Integer(stage)).toString()+","+cThreadName+"\n");
           f.close();
       } catch(Exception e) {
           System.err.println("Exception during writing the file: " +e);
           e.printStackTrace();
       }
	
   CID classID = new CID(context.getContractID(thread+1,stage+1));

	if (!(classID.toString().equals("org.mozilla.xpcom.CID@null"))){
        	iMThreadComponent component = loadComponent(classID);
	        if(component == null) {
        	    System.err.println("Can't load component");
	        } else {
	        component.initialize(context);
	        component.tHack(com);
	        (new ExecutionThread(component)).run(thread+1,stage+1);
		}
	}

       classID = new CID(context.getContractID(thread,stage+1));

	if (!(classID.toString().equals("org.mozilla.xpcom.CID@null"))){
        	iMThreadComponent component = loadComponent(classID);
	        if(component == null) {
        	    System.err.println("Can't load component");
	        } else {
	        component.initialize(context);
	        component.tHack(com);
		component.execute(thread,stage+1);
		}
	}
    }


    public Object queryInterface(IID iid) {
        System.out.println("DEbug:avm:MThreadComponent1::queryInterface iid="+iid);
        if ( iid.equals(nsISupports.IID)
             || iid.equals(iMThreadComponent1.IID)||iid.equals(iMThreadComponent.IID)) {
            return this;
        } else {
            return null;
        }
    }

    static CID iMThreadContextCID = new CID("139b8350-280a-46d0-8afc-f1939173c2ea");
    static {
      try {
          Class nsIComponentManagerClass = 
              Class.forName("org.mozilla.xpcom.nsIComponentManager"); 
          InterfaceRegistry.register(nsIComponentManagerClass);
          Class iMThreadComponentClass = 
              Class.forName("org.mozilla.xpcom.iMThreadComponent");
          InterfaceRegistry.register(iMThreadComponentClass);
          Class iMThreadContextClass = 
              Class.forName("org.mozilla.xpcom.iMThreadContext");
          InterfaceRegistry.register(iMThreadContextClass);
          Class nsIFactoryClass = 
              Class.forName("org.mozilla.xpcom.nsIFactory");
          InterfaceRegistry.register(nsIFactoryClass);
      } catch (Exception e) {
          e.printStackTrace();
      } catch (Error e) {
          e.printStackTrace();
      }
    }
}
