/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
package netscape.ldap.beans;

import java.awt.*;
import java.awt.event.*;
import java.beans.*;
import java.io.Serializable;

public class DisplayString extends TextArea implements Serializable {

    public DisplayString() {
        super();
        setEditable(false);
    }

    public void reportChange(PropertyChangeEvent evt) {
        Object obj = (Object)evt.getNewValue();
        if (obj == null) {
            append("null\n");
            return;
        }

        String[] values = null;
        if (obj instanceof String) {
            values = new String[1];
            values[0] = (String)obj;
        }
        else
            values = (String[])obj;

        int width = getSize().width - 10;
        Font f = getFont();
        for (int i=0; i<values.length; i++) {
            String text = values[i];
            if (f != null) {
                // Trim the text to fit.
                FontMetrics fm = getFontMetrics(f);
                while (fm.stringWidth(text) > width) {
                    text = text.substring(0, text.length()-1);
                }
            }
            append(text+'\n');
        }
    }

    public void clear (ActionEvent e) {
        setText("");
        invalidate();
        repaint();
    }
}
