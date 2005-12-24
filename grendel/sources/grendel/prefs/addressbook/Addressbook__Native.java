/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
package grendel.prefs.addressbook;

import grendel.prefs.Preferences;
import grendel.prefs.accounts.Account__Local;
import java.util.Properties;

/**
 *
 * @author hash9
 */
public abstract class Addressbook__Native extends AbstractAddressbook implements Addressbook
{
  
  /**
   * Creates a new instance of Addressbook_Native_Derby
   */
  public Addressbook__Native()
  {
    super();
  }
  
  /**
   * Creates a new instance of Addressbook_Native_Derby
   */
  public Addressbook__Native(String name)
  {
    super(name);
  }
  
  public String getSimpleName()
  {
    StringBuilder simple = new StringBuilder(getName());
    for (int i = 0;i<simple.length();i++)
    {
      char c = simple.charAt(i);
      if (Character.isWhitespace(c))
      {
        simple.setCharAt(i,'_');
      }
      else if (Character.isLetterOrDigit(c))
      {
        simple.setCharAt(i,Character.toLowerCase(c));
      }
      else
      {
        simple.deleteCharAt(i);
        i--; // so the new charator at this index will be tested.
      }
    }
    return simple.toString();
  }
  
  public static String getBaseDirectory()
  {
    return Preferences.getPreferances().getProfilePath().concat("/addressbook/");
  }
  
  public String getDirectory()
  {
    return getBaseDirectory().concat(getType().toLowerCase()).concat("/").concat(getSimpleName());
  }
  
  public String getSuperType()
  {
    return "jdbc";
  }
}
