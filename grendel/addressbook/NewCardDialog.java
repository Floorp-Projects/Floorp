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

class NewCardDialog extends JDialog {

    NewCardDialog(Frame aParent) {
        //FIX: Resource
        super(aParent, "Card for", true);

//        setLayout(new BoxLayout(this, BoxLayout.Y_AXIS));

        JTabbedPane tabbedPane = new JTabbedPane();
        getContentPane().add(tabbedPane, BorderLayout.CENTER);
        //add (tabbedPane);

        JComponent namePanel = createNamePanel ();
        tabbedPane.addTab("Name",null,namePanel,"Name Information");
//        add (namePanel);

        JComponent contactPanel = createContactPanel ();
        tabbedPane.addTab("Contact",null,contactPanel,"Contact Information");
//        add (contactPanel);

//        JComponent netConfPanel = createNetConfPanel ();
//        add (netConfPanel);

        setResizable(false);
        setSize (716, 515);

        addWindowListener(new AppCloser());
    }

    protected final class AppCloser extends WindowAdapter {
        public void windowClosing(WindowEvent e) {
            dispose();
        }
    }

    private JPanel createNamePanel () {
        //the outer most panel has groove etched into it.
        JPanel pane = new JPanel(false);
        pane.setLayout (new FlowLayout(FlowLayout.LEFT));

        JPanel namePane = new JPanel(false);
        namePane.setLayout (new GridLayout(3,2));

        makeField ("First Name:", 20, namePane);
        makeField ("Last Name:", 20, namePane);
        makeField ("Display Name:",20, namePane);

        pane.add (namePane);


        JPanel eMailPane = new JPanel(false);
        eMailPane.setLayout (new GridLayout (2,2));

        makeField ("Email Address:", 20, eMailPane);
        makeField ("Nick Name:", 20, eMailPane);

        pane.add (eMailPane);


        JPanel phonePane = new JPanel(false);
        phonePane.setLayout (new GridLayout (5,2));

        makeField ("Work:", 20, phonePane);
        makeField ("Home:", 20, phonePane);
        makeField ("Fax:", 20, phonePane);
        makeField ("Pager:", 20, phonePane);
        makeField ("Cellular:", 20, phonePane);

        pane.add (phonePane);

        return pane;
    }

    private JPanel createContactPanel () {
        //the outer most panel has groove etched into it.
        JPanel pane = new JPanel(false);
        pane.setLayout (new FlowLayout(FlowLayout.LEFT));

        JPanel contactPane = new JPanel(false);
        contactPane.setLayout (new GridLayout (3,2));

        makeField ("Title:", 20, contactPane);
        makeField ("Organization:", 20, contactPane);
        makeField ("Department:", 20, contactPane);

        pane.add (contactPane);

        JPanel addressPane = new JPanel(false);
        addressPane.setLayout (new GridLayout (6,2));

        makeField ("Address:", 20, addressPane);
        makeField ("City:", 20, addressPane);
        makeField ("State:", 20, addressPane);
        makeField ("ZIP:", 20, addressPane);
        makeField ("Country:", 20, addressPane);
        makeField ("URL:", 20, addressPane);

        pane.add (addressPane);

        return pane;
    }

    private void makeField (String aTitle, int aCol, JPanel aPanel) {
        JLabel title = new JLabel (aTitle);
        aPanel.add (title);

        JTextField textField = new JTextField (aCol);
        aPanel.add (textField);
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
