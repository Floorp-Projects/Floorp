/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Client QA Team, St. Petersburg, Russia
 */


 
package org.mozilla.webclient.test.basic;

/*
 * ChekingThread.java
 */

public class CheckingThread extends Thread {
    int timeout = Integer.MAX_VALUE;
    TestContext context = null;
    TestRunner  runner = null;

    public CheckingThread(int timeout,TestRunner runner, TestContext context){
        this.context = context;
        this.runner = runner;
        this.timeout = timeout;
    }

    public void run(){ 
        System.out.println("CheckThread sleep to " + timeout);
        try {
            sleep(timeout);
        } catch (Exception e) {
            System.out.println("Checking thread had been interrupted");
        }
        System.out.println("prepare to exit ");
        context.delete();
        System.out.println("Context.delete completed");
	    runner.delete();
    }

}
