/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

package grendel.addressbook;

import java.awt.*;
import java.awt.event.*;
import java.util.*;

import javax.swing.*;

class NewCardDialog extends Dialog {

    NewCardDialog(Frame aParent) {
        //FIX: Resource
        super(aParent, "Card for", true);

//        setLayout(new BoxLayout(this, BoxLayout.Y_AXIS));

        JComponent namePanel = createNamePanel ();
        add (namePanel);

//        JComponent contactPanel = createContactPanel ();
//        add (contactPanel);

//        JComponent netConfPanel = createNetConfPanel ();
//        add (netConfPanel);

        setResizable(false);
        setSize (716, 515);
    }

    private JPanel createNamePanel () {
        //the outer most panel has groove etched into it.
        JPanel pane = new JPanel(false);
//        pane.setLayout (new BoxLayout(pane, BoxLayout.Y_AXIS));

        JTextField mFirstName = makeField ("First Name:", 20);
        pane.add (mFirstName);
/*
        JTextField mLastName = makeField ("Last Name:", 20);
        pane.add (mLastName);

        JTextField mOrganization = makeField ("Organization:", 20);
        pane.add (mOrganization);

        JTextField mTitle = makeField ("Title:", 20);
        pane.add (mTitle);

        JTextField mEmail = makeField ("Email Address:", 20);
        pane.add (mEmail);
*/
        return pane;
    }

    private JTextField makeField (String aTitle, int aCol) {
//        JPanel box = new JPanel (false);
        Box box = new Box (BoxLayout.X_AXIS);

        JLabel title = new JLabel (aTitle);
        title.setAlignmentY((float)0.0);       //bottom
        title.setAlignmentX((float)0.0);       //left
        box.add (title);

        JTextField textField = new JTextField (aCol);
        textField.setAlignmentY((float)0.0);       //bottom
        textField.setAlignmentX((float)1.0);       //right
        box.add (textField);

        return textField;
    }
}
