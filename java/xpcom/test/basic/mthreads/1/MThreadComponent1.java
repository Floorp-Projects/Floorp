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

    public MThreadComponent1() {
        System.out.println("DEbug:avm:MThreadComponent1 constructor");
    }

    
    public void initialize(iMThreadContext context) {
        System.out.println("DEbug:avm:MThreadComponent1:initialize Thread is " + Thread.currentThread().getName());
        this.context = context;
    }
    public iMThreadComponent loadComponent(CID classID) {
        System.out.println("DEbug:avm:MThreadComponent1:loadComponent");
        nsIComponentManager cm = context.getComponentManager();
        nsIFactory factory = cm.findFactory(classID);
        if(factory == null) {
            System.out.println("DEbug:avm:Factory is NULL!!!!!");
            return null;
        }
        iMThreadComponent component = (iMThreadComponent)factory.createInstance(null, iMThreadComponent.IID);
        return component;
    }

    /* void Execute (); */
    public void execute(String tName) {
        System.out.println("DEbug:avm:MThreadComponent1:execute.");
        String cThreadName = Thread.currentThread().getName();
        if(tName.equals(cThreadName)) {
            System.out.println("PASSED: original and expected names equals to " + tName);
        } else {
            System.out.println("FAILED: Current thread name is " + cThreadName + "  instead of " + tName );
        }
        CID classID = new CID(context.getNext());
        iMThreadComponent component = loadComponent(classID);
        if(component == null) {
            System.out.println("Can't load component");
            return;
        }
        System.out.println("Initialize of " + component + " called in thread " + cThreadName);
        component.initialize(context);
        (new ExecutionThread(component)).run();
        //component.execute();
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

    static CID J2XINServerCID = new CID("1ddc5b10-9852-11d4-aa22-00a024a8bbac");
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








