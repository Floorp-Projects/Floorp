/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Will Scullin <scullin@netscape.com>, 15 Oct 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.prefs;

import java.util.StringTokenizer;

import javax.mail.URLName;

import javax.swing.UIManager;
import javax.swing.UnsupportedLookAndFeelException;

import calypso.util.Preferences;
import calypso.util.PreferencesFactory;

import grendel.ui.StoreFactory;
import java.util.Vector;

public class Prefs {
  static Preferences   fPrefs = PreferencesFactory.Get();

  static final String kOrganization     = "mail.identity.organization.";
  static final String kEmailAddress     = "mail.identity.email.";
  static final String kSignatureFile    = "mail.identity.signature.";
  static final String kUserName         = "mail.identity.username.";
  static final String kPopLeaveOnServer = "pop.leaveMailOnServer";
  static final String kMailDirectory    = "mail.directory";
  static final String kSMTPHost         = "mail.identity.smtphost.0";
  static final String kStoreList        = "mail.storelist";

  static final String kLocalProtocol = "berkeley";
  static final String kUserPrefsCount = "mail.identities";

  Vector ids = new Vector();

  public Prefs() {
    int total = 1; //getUserPrefsCount();
    System.out.println("Total is " + total);
    for (int i = 0; i < total; i++) {
      addUserPrefs(readUserPrefs(i));
    }
  }
  
  public int getUserPrefsCount() {
    return Integer.parseInt(fPrefs.getString(kUserPrefsCount, "1"));
  }
  

  public UserPrefs getUserPrefs() {
    return getUserPrefs(0);
  }

  public UserPrefs getUserPrefs(int count) {
    return (UserPrefs)ids.elementAt(count);
  }
  

  public UserPrefs readUserPrefs(int count) {
    UserPrefs res = new UserPrefs();
    res.setUserName(fPrefs.getString(kUserName + count, 
				     "John Doe"));
    res.setUserEmailAddress(fPrefs.getString(kEmailAddress + count, 
                                             "john@doe.com"));
    res.setUserOrganization(fPrefs.getString(kOrganization + count, 
                                             ""));
    res.setSignatureFile(fPrefs.getString(kSignatureFile + count, 
                                             ""));

    return res;    
  }  

  public synchronized void setUserPrefs(UserPrefs aPrefs) {
    addUserPrefs(aPrefs);
  }

  public synchronized void addUserPrefs(UserPrefs aPrefs) {
    int location = 0;

    // we need to find out where the object is located for its suffix
    if (ids.contains(aPrefs)) {
      location = ids.indexOf(aPrefs);
    } else {
      location = 0;
    }
    
    
    fPrefs.putString(kUserName + location,
                     aPrefs.getUserName());
    fPrefs.putString(kEmailAddress + location, 
                     aPrefs.getUserEmailAddress());
    fPrefs.putString(kOrganization + location, 
                     aPrefs.getUserOrganization());
    fPrefs.putString(kSignatureFile + location, 
                     aPrefs.getSignatureFile());    
    ids.addElement(aPrefs);
  }

  public MailServerPrefs getMailServerPrefs() {
    MailServerPrefs res = new MailServerPrefs();

    res.setMailDirectory(fPrefs.getString(kMailDirectory, ""));
    res.setLeaveOnServer(fPrefs.getBoolean(kPopLeaveOnServer, false));
    res.setSMTPHost(fPrefs.getString(kSMTPHost, "mail"));

    String storelist = fPrefs.getString(kStoreList, "");
    StringTokenizer st = new StringTokenizer(storelist, " ,;");

    URLName urlNames[] = new URLName[st.countTokens()];
    int i = 0;
    while (st.hasMoreTokens()) {
      String storename = st.nextToken().trim();
      URLName urlName = null;
      if (storename.indexOf(":") != -1) {
        urlName = new URLName(storename);
      } else {
        urlName = new URLName(storename, null, -1, null, null, null);
      }
      urlNames[i++] = urlName;
    }

    res.setStores(urlNames);

    return res;
  }

  public void setMailServerPrefs(MailServerPrefs aPrefs) {
    fPrefs.putString(kMailDirectory, aPrefs.getMailDirectory());
    fPrefs.putBoolean(kPopLeaveOnServer, aPrefs.getLeaveOnServer());
    fPrefs.putString(kSMTPHost, aPrefs.getSMTPHost());

    URLName stores[] = aPrefs.getStores();
    boolean berkeley = false;
    int i;

    StringBuffer buffer = new StringBuffer();
    for (i = 0; i < stores.length; i++) {
      if (stores[i].getProtocol().equals(kLocalProtocol)) {
        berkeley = true;
        if (aPrefs.getMailDirectory().equals("")) {
          continue;
        }
      }
      if (i > 0) {
        buffer.append(",");
      }
      if (stores[i].getHost() == null) {
        buffer.append(stores[i].getProtocol());
      } else {
        buffer.append(stores[i].toString());
      }
    }

    if (!berkeley && !aPrefs.getMailDirectory().equals("")) {
      if (i > 0) {
        buffer.append(",");
      }
      buffer.append(kLocalProtocol);
    }

    System.out.println("putting " + kStoreList + ":" + buffer.toString());
    fPrefs.putString(kStoreList, buffer.toString());

    StoreFactory.Instance().refreshStores();
  }

  public UIPrefs getUIPrefs() {
    UIPrefs res = new UIPrefs();

    res.setLAF(UIManager.getLookAndFeel());

    return res;
  }

  public void setUIPrefs(UIPrefs aUIPrefs) {
    if (aUIPrefs.getLAF() != null) {
      try {
        UIManager.setLookAndFeel(aUIPrefs.getLAF());
      } catch (UnsupportedLookAndFeelException e) {
        e.printStackTrace();
      }
    }
  }

  public void set(Object obj) {
    if (obj instanceof UserPrefs) {
      setUserPrefs((UserPrefs)obj);
    } else if (obj instanceof UIPrefs) {
      setUIPrefs((UIPrefs)obj);
    } else if (obj instanceof MailServerPrefs) {
      setMailServerPrefs((MailServerPrefs)obj);
    }
  }
}
