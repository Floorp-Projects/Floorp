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


public class StressComponent1 implements iStressComponent1 {

    public StressComponent1() {
        System.out.println("DEbug:avm:StressComponent1 constructor");
    }

    public void reCall(iStressComponent2 st2) {
        System.out.println("DEbug:avm:StressComponent1 reCall");
        st2.increase();
    }

    public Object queryInterface(IID iid) {
        System.out.println("DEbug:avm:StressComponent1::queryInterface iid="+iid);
        if ( iid.equals(nsISupports.IID)
             || iid.equals(iStressComponent1.IID)) {
            return this;
        } else {
            return null;
        }
    }

    static {
      try {
          Class iStressComponent2Class = 
              Class.forName("org.mozilla.xpcom.iStressComponent2");
          InterfaceRegistry.register(iStressComponent2Class);
      } catch (Exception e) {
          e.printStackTrace();
      } catch (Error e) {
          e.printStackTrace();
      }
    }
}








