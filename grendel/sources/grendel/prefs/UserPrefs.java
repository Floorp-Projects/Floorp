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
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 */

package grendel.prefs;

public class UserPrefs {
  String fUserName;
  String fUserEmailAddress;
  String fUserOrganization;
  String fSignatureFile;

  public String getUserName() {
    return fUserName;
  }

  public void setUserName(String aName) {
    fUserName = aName;
  }

  public String getUserEmailAddress() {
    return fUserEmailAddress;
  }

  public void setUserEmailAddress(String aAddress) {
    fUserEmailAddress = aAddress;
  }

  public String getUserOrganization() {
    return fUserOrganization;
  }

  public void setUserOrganization(String aOrganization) {
    fUserOrganization = aOrganization;
  }

  public String getSignatureFile() {
    return fSignatureFile;
  }

  public void setSignatureFile(String file) {
    fSignatureFile = file;
  }

  public boolean equals(Object aObject) {
    if (aObject instanceof UserPrefs) {
      UserPrefs prefs = (UserPrefs) aObject;
      return prefs.fUserName.equals(fUserName) &&
        prefs.fUserEmailAddress.equals(fUserEmailAddress) &&
        prefs.fUserOrganization.equals(fUserOrganization) &&
        prefs.fSignatureFile.equals(fSignatureFile);
    }
    return false;
  }
}
