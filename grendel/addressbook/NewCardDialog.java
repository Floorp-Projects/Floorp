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
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Mauro Botelho <mabotelh@bellsouth.net>
 */

package grendel.addressbook;

import grendel.addressbook.addresscard.*;

// *********************************************************berkeley test stuff

import javax.mail.Authenticator;
import javax.mail.PasswordAuthentication;
import javax.mail.Session;
import javax.mail.Folder;
import javax.mail.MessagingException;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.StringWriter;
import java.io.PrintWriter;

import grendel.storage.BerkeleyStore;

// ********************************************************End of berkeley test

import java.awt.*;
import java.awt.event.*;
import java.util.*;

import javax.swing.*;

class NewCardDialog extends JDialog {

    private JTextField eFirstName;
    private JTextField eLastName;
    private JTextField eDisplayName;
    private JTextField eEMailAddress;
    private JTextField eNickName;
    private JTextField eWorkPhone;
    private JTextField eHomePhone;
    private JTextField eFax;
    private JTextField ePager;
    private JTextField eCellPhone;

    private JTextField eTitle;
    private JTextField eOrganization;
    private JTextField eDepartment;
    private JTextField eAddress;
    private JTextField eCity;
    private JTextField eState;
    private JTextField eZIP;
    private JTextField eCountry;
    private JTextField eURL;


//  *************************************************************Berkeley Stuff

  /** The Properties instance that all the javamail stuff will use.  We try not
    to use calypso.util.Preferences during SelfTest, because there's no real
    way to control the values to be found there.   Typically, your SelfTest
    code will stuff values into this Properties database so that the Store
    you're using will pull those values out. */
  static protected Properties props;

  /** A stupid authenticator that we use to stuff in name/password info into
    our tests. */
  static private StupidAuthenticator authenticator;

  /** The javax.mail.Session object.  This is created for you by startTests. */
  static protected Session session;

  /** The directory where you can store temporary stuff.  If you want to use
    this, be sure to call makePlayDir() at the beginning of your test; that
    will ensure that the directory exists and is empty. */
  static protected File playdir = new File("selftestdir");

// *******************************************************End of berkeley stuff

    NewCardDialog(Frame aParent) {
        //FIX: Resource
        super(aParent, "Card for", true);


        JTabbedPane tabbedPane = new JTabbedPane();
        getContentPane().add(tabbedPane, BorderLayout.CENTER);

        JComponent namePanel = createNamePanel ();
        tabbedPane.addTab("Name",null,namePanel,"Name Information");

        JComponent contactPanel = createContactPanel ();
        tabbedPane.addTab("Contact",null,contactPanel,"Contact Information");

//        JComponent netConfPanel = createNetConfPanel ();
//        add (netConfPanel);

        setResizable(true);
        setSize (300, 515);

        addWindowListener(new AppCloser());

        JButton bOK = new JButton("OK");
        bOK.addActionListener(new OKEvents());

        JButton bCancel = new JButton("Cancel");
        bCancel.addActionListener(new CancelEvents());

        JPanel pOKCancel = new JPanel();
        pOKCancel.add(bOK);
        pOKCancel.add(bCancel);

        getContentPane().add(pOKCancel, BorderLayout.SOUTH);

        getRootPane().setDefaultButton(bOK);
    }

    protected final class AppCloser extends WindowAdapter {
        public void windowClosing(WindowEvent e) {
            dispose();
        }
    }

    protected final class OKEvents implements ActionListener {
        public void actionPerformed(ActionEvent e) {

           ACS_Personal myAddressBook = new ACS_Personal("abook.nab",true);
           AddressCardAttribute anAttribute;
           AddressCardAttributeSet anAttributeSet = new AddressCardAttributeSet();
           AddressCard aCard;

           anAttribute = new AddressCardAttribute("cn",eFirstName.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("sn",eLastName.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("mail",eEMailAddress.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("city",eCity.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("telephoneNumber",eHomePhone.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("workPhone",eWorkPhone.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("fax",eFax.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("pager",ePager.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("cellular",eCellPhone.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("Title",eTitle.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("o",eOrganization.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("department",eDepartment.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("postalAddress",eAddress.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("city",eCity.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("state",eState.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("postalCode",eZIP.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("c",eCountry.getText());
           anAttributeSet.add(anAttribute);
           anAttribute = new AddressCardAttribute("url",eURL.getText());
           anAttributeSet.add(anAttribute);

           aCard = new AddressCard(myAddressBook,anAttributeSet);
           myAddressBook.add(aCard, true);

           myAddressBook.close();

           System.out.println("First Name: " + eFirstName.getText());

           dispose();
        }
    }

    protected final class CancelEvents implements ActionListener{
        public void actionPerformed(ActionEvent e) {
           dispose();
        }
    }

    private JPanel createNamePanel () {
        //the outer most panel has groove etched into it.
        JPanel pane = new JPanel(false);
        pane.setLayout (new FlowLayout(FlowLayout.LEFT));

        JPanel namePane = new JPanel(false);
        namePane.setLayout (new GridLayout(3,2));

        eFirstName   = makeField ("First Name:", 20, namePane);
        eLastName    = makeField ("Last Name:", 20, namePane);
        eDisplayName = makeField ("Display Name:",20, namePane);

        pane.add (namePane);


        JPanel eMailPane = new JPanel(false);
        eMailPane.setLayout (new GridLayout (2,2));

        eEMailAddress = makeField ("Email Address:", 20, eMailPane);
        eNickName = makeField ("Nick Name:", 20, eMailPane);

        pane.add (eMailPane);


        JPanel phonePane = new JPanel(false);
        phonePane.setLayout (new GridLayout (5,2));

        eWorkPhone = makeField ("Work:", 20, phonePane);
        eHomePhone = makeField ("Home:", 20, phonePane);
        eFax       = makeField ("Fax:", 20, phonePane);
        ePager     = makeField ("Pager:", 20, phonePane);
        eCellPhone = makeField ("Cellular:", 20, phonePane);

        pane.add (phonePane);

        return pane;
    }

    private JPanel createContactPanel () {
        //the outer most panel has groove etched into it.
        JPanel pane = new JPanel(false);
        pane.setLayout (new FlowLayout(FlowLayout.LEFT));

        JPanel contactPane = new JPanel(false);
        contactPane.setLayout (new GridLayout (3,2));

        eTitle        = makeField ("Title:", 20, contactPane);
        eOrganization = makeField ("Organization:", 20, contactPane);
        eDepartment   = makeField ("Department:", 20, contactPane);

        pane.add (contactPane);

        JPanel addressPane = new JPanel(false);
        addressPane.setLayout (new GridLayout (6,2));

        eAddress      = makeField ("Address:", 20, addressPane);
        eCity         = makeField ("City:", 20, addressPane);
        eState        = makeField ("State:", 20, addressPane);
        eZIP          = makeField ("ZIP:", 20, addressPane);
        eCountry      = makeField ("Country:", 20, addressPane);
        eURL          = makeField ("URL:", 20, addressPane);

        pane.add (addressPane);

        return pane;
    }

    private JTextField makeField (String aTitle, int aCol, JPanel aPanel) {
        JLabel title = new JLabel (aTitle);
        aPanel.add (title);

        JTextField textField = new JTextField (aCol);
        aPanel.add (textField);

        return textField;
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

class StupidAuthenticator extends Authenticator {
  String user;
  String password;

  StupidAuthenticator() {}

  void set(String u, String p) {
    user = u;
    password = p;
  }

  protected PasswordAuthentication getPasswordAuthentication() {
    return new PasswordAuthentication(user, password);
  }
}
