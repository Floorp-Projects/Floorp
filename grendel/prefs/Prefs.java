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

public class Prefs {
  static Preferences   fPrefs = PreferencesFactory.Get();

  static final String kUserNamePre = "mail.identity.username.";
  static final String kOrganization = "mail.identity.organization.";
  static final String kEmailAddress = "mail.identity.email.";
  static final String kSignatureFile = "mail.identity.signature.0";
  static final String kPopLeaveOnServer = "pop.leaveMailOnServer";
  static final String kMailDirectory = "mail.directory";
  static final String kSMTPHost = "mail.identity-0.smtphost";
  static final String kStoreList = "mail.storelist";
  static final String kUserName = "mail.identity.username.0";

  static final String kLocalProtocol = "berkeley";
  static final String kUserPrefsCount = "mail.identities";
  
  public int getUserPrefsCount() {
    return Integer.parseInt(fPrefs.getString(kUserPrefsCount, "0"));
  }
  

  public UserPrefs getUserPrefs() {
    return getUserPrefs(0);
  }

  public UserPrefs getUserPrefs(int count) {
    UserPrefs res = new UserPrefs();
    res.setUserName(fPrefs.getString(kUserName + count, 
				     "John Doe"));
    res.setUserName(fPrefs.getString(kEmailAddress + count, 
				     "john@doe.com"));
    res.setUserName(fPrefs.getString(kOrganization + count, 
				     ""));

    return res;    
  }
  

  public void setUserPrefs(UserPrefs aPrefs) {
    fPrefs.putString(kUserName, aPrefs.getUserName());
    fPrefs.putString(kEmailAddress, aPrefs.getUserEmailAddress());
    fPrefs.putString(kOrganization, aPrefs.getUserOrganization());
    fPrefs.putString(kSignatureFile, aPrefs.getSignatureFile());
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
        System.out.println("Setting L&F to " + aUIPrefs.getLAF());
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
