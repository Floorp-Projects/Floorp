/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This file has been contributed to Mozilla.
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
 * Created: Jeff Galyan <jeffrey.galyan@sun.com>, 2 Jan 1999
 */

package grendel.widgets;

import java.awt.Container;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Toolkit;

import javax.swing.JPanel;

/** Spring provides a lightweight component which participates in layout
 *  but has no view.
 *  A Spring automatically sets its size to fill the space between it and the 
 *  other Components in a Container, pushing the other components to either 
 *  side, as needed.
 * 
 *  @author Jeff Galyan <jeffrey.galyan@sun.com>
 *  @see Container
 */

public class Spring extends Container {

    public static final int HORIZONTAL = 0;
    public static final int VERTICAL = 1;

    public Spring() {
	super();

	add(new JPanel());
     
    }

}
