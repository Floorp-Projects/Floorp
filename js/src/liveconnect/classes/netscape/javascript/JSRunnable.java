/* -*- Mode: Java; tab-width: 8; c-basic-offset: 4 -*-
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

package netscape.javascript;

/**
 * Runs a JavaScript object with a run() method in a separate thread.
 */
public class JSRunnable implements Runnable {
	private JSObject runnable;

	public JSRunnable(JSObject runnable) {
		this.runnable = runnable;
		synchronized(this) {
			new Thread(this).start();
			try {
				this.wait();
			} catch (InterruptedException ie) {
			}
		}
	}
	
	public void run() {
		try {
			runnable.call("run", null);
			synchronized(this) {
				notifyAll();
			}
		} catch (Throwable t) {
			System.err.println(t);
			t.printStackTrace(System.err);
		}
	}
}
