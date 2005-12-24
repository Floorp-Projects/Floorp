/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * LocalAccount.java
 *
 * Created on 09 August 2005, 22:31
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package grendel.prefs.accounts;

import grendel.prefs.xml.*;

/**
 *
 * @author hash9
 */
public abstract class Account__Local extends AbstractAccount implements Account__Receive {
  
  /**
   * Creates a new instance of Account__Local
   */
  public Account__Local() {
    super();
  }
  
  /**
   * Creates a new instance of Account__Local
   */
  public Account__Local(String name) {
    super(name);
  }
  
  public Account__Send getSendAccount() {
    XMLPreferences pref = this.getPropertyPrefs("send_account");
    if (pref instanceof Account__Send) {
      return (Account__Send) pref;
    }
    return null;
  }
  
  public void setSendAccount(Account__Send a) {
    if (a instanceof AbstractAccount) {
      this.setProperty("send_account",(AbstractAccount) a);
    } else {
      throw new IllegalArgumentException("Account__Send object "+a.toString()+" is not an instance of "+AbstractAccount.class.toString());
    }
  }
  
  public java.util.Properties setSessionProperties(java.util.Properties props) {
    return props;
  }
  
  public String getStoreDirectory() {
    return this.getPropertyString("store_dir");
  }
  
  public void setStoreDirectory(String dir) {
    this.setProperty("store_dir",dir);
  }
  
  public String getStoreInbox() {
    String inbox =  this.getPropertyString("store_inbox");
    if (inbox==null) {
      inbox = getStoreDirectory();
      if (inbox!=null) {
        inbox += java.io.File.separatorChar + getInboxName();
      }
    }
    return inbox;
  }
  
  public void setStoreInbox(String dir) {
    this.setProperty("store_inbox",dir);
  }
  
  public abstract String getInboxName();
  
}
