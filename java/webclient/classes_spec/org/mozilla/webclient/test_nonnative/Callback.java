/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ashutosh Kulkarni <ashuk@eng.sun.com>
 *
 */


package org.mozilla.webclient.test_nonnative;

import ice.storm.*;
import java.awt.*;
import java.awt.event.*;

import java.util.Hashtable;


/**
* A callback class to create new top level windows for the browser.
*/
public class Callback implements StormCallback {

	private StormBase base;

	private Hashtable windows = new Hashtable();

	public void init(StormBase base) {
		this.base=base;
	}

	public Container createTopLevelContainer(Viewport viewport) {
		EMWindow f = new EMWindow(base,viewport);
		windows.put(viewport.getId(),f);
		f.setVisible(true);
		return f.getPanel();	
	}

	public void disposeTopLevelContainer(Viewport viewport) {
		Window w = (Window)windows.get(viewport.getId());
		if (w!=null) {
			windows.remove(viewport.getId());
			w.setVisible(false);
			w.dispose();
			if (windows.isEmpty()) {
				System.exit(0);
			}
		}
	}

	public void message(Viewport vp, String msg) {
//		newBox().showMessageBox(vp, msg);
	}

	public boolean confirm(Viewport vp, String msg) {
//		return newBox().showConfirmBox(vp, msg);
		return true;
	}

	public String prompt(Viewport vp, String msg, String defVal) {
//		return newBox().showPromptBox(vp, msg, defVal);
		return null;
	}
	
//	private BoxDialogs newBox() { return new BoxDialogs_awt(); }

}
