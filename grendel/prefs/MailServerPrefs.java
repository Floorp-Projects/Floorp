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
 * Created: Will Scullin <scullin@netscape.com>, 16 Oct 1997.
 */

package grendel.prefs;

import javax.mail.URLName;

public class MailServerPrefs {
  String fMailHost;
  String fMailUser;
  String fMailPassword;
  String fMailDirectory;
  boolean fLeaveOnServer;
  String fSMTPHost;

  URLName fStores[];

  public void setStores(URLName aStores[]) {
    fStores = aStores;
  }

  public URLName[] getStores() {
    return fStores;
  }

  public void setMailDirectory(String aDir) {
    fMailDirectory = aDir;
  }

  public String getMailDirectory() {
    return fMailDirectory;
  }

  public void setLeaveOnServer(boolean aLeave) {
    fLeaveOnServer = aLeave;
  }

  public boolean getLeaveOnServer() {
    return fLeaveOnServer;
  }

  public void setSMTPHost(String aHost) {
    fSMTPHost = aHost;
  }

  public String getSMTPHost() {
    return fSMTPHost;
  }
}
