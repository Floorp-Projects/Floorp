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
 * The Initial Developer of the Original Code is Edwin Woudt 
 * <edwin@woudt.nl>.  Portions created by Edwin Woudt are
 * Copyright (C) 1999 Edwin Woudt. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package grendel.prefs.base;

public class ServerStructure {

  String myDescription = "";
  String myHost = "";
  int myPort = -1;
  String myType = "";
  String myUsername = "";
  String myPassword = "";
  int myDefaultIdentity = 0;
  
  String myBerkeleyDirectory = "";
  
  boolean myPOP3ShowAsImap = true;
  boolean myPOP3LeaveMailOnServer = false;
  
  public ServerStructure() {
  }

  public ServerStructure(String aDescription) {
    
    myDescription = aDescription;
    
  }
  
  public String getDescription() {
    return myDescription;
  }

  public String getHost() {
    return myHost;
  }

  public int getPort() {
    return myPort;
  }

  public String getType() {
    return myType;
  }

  public String getUsername() {
    return myUsername;
  }

  public String getPassword() {
    return myPassword;
  }

  public int getDefaultIdentity() {
    return myDefaultIdentity;
  }

  public String getBerkeleyDirectory() {
    return myBerkeleyDirectory;
  }

  public boolean getPOP3LeaveMailOnServer() {
    return myPOP3LeaveMailOnServer;
  }

  public boolean getPOP3ShowAsImap() {
    return myPOP3ShowAsImap;
  }

  public void setDescription(String aDescription) {
    myDescription = aDescription;
  }

  public void setHost(String aHost) {
    myHost = aHost;
  }

  public void setPort(int aPort) {
    myPort = aPort;
  }

  public void setType(String aType) {
    myType = aType;
  }

  public void setUsername(String aUsername) {
    myUsername = aUsername;
  }

  public void setPassword(String aPassword) {
    myPassword = aPassword;
  }

  public void setDefaultIdentity(int aDefaultIdentity) {
    myDefaultIdentity = aDefaultIdentity;
  }

  public void setBerkeleyDirectory(String aBerkeleyDirectory) {
    myBerkeleyDirectory = aBerkeleyDirectory;
  }

  public void setPOP3LeaveMailOnServer(boolean aPOP3LeaveMailOnServer) {
    myPOP3LeaveMailOnServer = aPOP3LeaveMailOnServer;
  }

  public void setPOP3ShowAsImap(boolean aPOP3ShowAsImap) {
    myPOP3ShowAsImap = aPOP3ShowAsImap;
  }

}