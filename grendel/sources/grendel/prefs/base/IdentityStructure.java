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

public class IdentityStructure {

  String myDescription = "";
  String myName = "";
  String myEMail = "";
  String myReplyTo = "";
  String myOrganization = "";
  String mySignature = "";
  
  public IdentityStructure() {
  }

  public IdentityStructure(String aDescription) {
    
    myDescription = aDescription;
    
  }
  
  public String getDescription() {
    return myDescription;
  }

  public String getName() {
    return myName;
  }

  public String getEMail() {
    return myEMail;
  }

  public String getReplyTo() {
    return myReplyTo;
  }

  public String getOrganization() {
    return myOrganization;
  }

  public String getSignature() {
    return mySignature;
  }
  
  public void setDescription(String aDescription) {
    myDescription = aDescription;
  }

  public void setName(String aName) {
    myName = aName;
  }

  public void setEMail(String aEMail) {
    myEMail = aEMail;
  }

  public void setReplyTo(String aReplyTo) {
    myReplyTo = aReplyTo;
  }

  public void setOrganization(String aOrganization) {
    myOrganization = aOrganization;
  }

  public void setSignature(String aSignature) {
    mySignature = aSignature;
  }

}