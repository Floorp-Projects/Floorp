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

    Test program for the Composition Editor.

    This program's only function is to get a SMTP server name and
    a mail user name then perform the following two lines...

        Composition CompFrame = new Composition(mailServerName, userName);
        CompFrame.show();

    (look near the bottom of this file)
 */

package grendel.composition;

import javax.swing.text.*;
import javax.swing.*;

import java.awt.event.*;
import java.awt.*;

class CompositionTest extends JPanel implements ActionListener {
    JTextField  mServerName;
    JTextField  mUserName;
    JButton     mSendMailButton;

    public static void main(String[] args) {
        JFrame frame = new JFrame("Composition test");
        frame.addWindowListener(new AppCloser());
        frame.setLayout (new BorderLayout ());
        frame.add (new CompositionTest(), BorderLayout.CENTER);
        //frame.setSize(450, 200);
        frame.pack();
        frame.show();
    }

    /**
     * Closes the appliction.
     */
    protected static final class AppCloser extends WindowAdapter {
        public void windowClosing(WindowEvent e) {
            System.exit(0);
        }
    }

    CompositionTest () {
        super (true);

        setBackground (Color.lightGray);
        setLayout(new FlowLayout());

        add (new JLabel("Mail Server:"));
        mServerName = new JTextField ("nsmail-2.mcom.com", 25);
        add (mServerName);

        add (new JLabel("User Name:"));
        mUserName = new JTextField ("", 25);
        add (mUserName);

        mSendMailButton = new JButton ("Send Mail");
        add (mSendMailButton);
        mSendMailButton.addActionListener (this);
    }

    public void actionPerformed(ActionEvent evt) {
        if (mSendMailButton == evt.getSource()) {
            //check fields
            String mailServerName = mServerName.getText().trim();

            if (mailServerName.equals("")) {
                System.out.println ("Please enter mail server name.");
                return;
            }

            String userName = mUserName.getText().trim();

            if (userName.equals("")) {
                System.out.println ("Please enter a user name.");
                return;
            }

            //This is where the compsiton editor is started.
            Composition CompFrame = new Composition();
            CompFrame.pack();
            CompFrame.show();
        }
    }
}
