/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This file has been contributed to the Mozilla project by Jeff Galyan
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Jeff Galyan <jeffrey.galyan@sun.com>, 1 January 1999
 * 
 */

package grendel.ui;

import java.awt.event.ActionEvent;
import javax.swing.AbstractAction;

public class UIAction extends AbstractAction {

    private String name;

    public UIAction(String aName) {
	super(aName);
	name = aName;
    }


    public void actionPerformed(ActionEvent e) {
    }

    public String getName() {
	return name;
    }

}
