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
import java.util.Date;


public class ExecutionThread extends Thread {
    public static int MAX_TIMEOUT = 20000;
    int timeout = Integer.MAX_VALUE;
    public static long ID = 0;
    iMThreadComponent component;

    public ExecutionThread(iMThreadComponent component){
        this.ID = new Date().getTime();
        this.setName(Long.toHexString(ID));
        
        this.component = component;
    }
    public ExecutionThread(iMThreadComponent component, int timeout){
        this.ID = new Date().getTime();
        this.setName(Long.toHexString(ID));
        this.component = component;
        if(timeout < MAX_TIMEOUT) {
            this.timeout = timeout;
        }else {
            this.timeout = MAX_TIMEOUT;
        }
    }
    public int timeoutGen() {
        return (int)Math.round(Math.random()*MAX_TIMEOUT);
    }
    public void run(int t, int s){ 
        try {
            if(timeout == Integer.MAX_VALUE) {
                timeout = timeoutGen();
            }
            sleep(timeout);
            component.execute(t,s);
        } catch (Exception e) {
            System.out.println("Execution thread had been interrupted");
        }
    }

}
