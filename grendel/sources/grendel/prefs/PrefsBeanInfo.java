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
 */

package grendel.prefs;

import java.beans.Introspector;
import java.beans.IntrospectionException;
import java.beans.PropertyDescriptor;
import java.beans.SimpleBeanInfo;
import java.util.ResourceBundle;
import java.util.MissingResourceException;

public class PrefsBeanInfo extends SimpleBeanInfo {

  static final String sProps[] = {"UserPrefs",
                                  "MailServerPrefs",
                                  "UIPrefs"
  };

  public PropertyDescriptor[] getPropertyDescriptors() {
    PropertyDescriptor res[] = null;

    ResourceBundle labels =
      ResourceBundle.getBundle("grendel.prefs.PrefLabels");

    try {
      res = new PropertyDescriptor[sProps.length];
      for (int i = 0; i < sProps.length; i++) {
        PropertyDescriptor propDesc =
          new PropertyDescriptor(Introspector.decapitalize(sProps[i]),
                                 Prefs.class,
                                 "get" + sProps[i],
                                 "set" + sProps[i]);
        String label = sProps[i];
        try {
          label = labels.getString(Introspector.decapitalize(sProps[i]) +
                                   "Label");
        } catch (MissingResourceException e) {}

        propDesc.setDisplayName(label);
        try {
          propDesc.setPropertyEditorClass(Class.forName("grendel.prefs." +
                                                        sProps[i] + "Editor"));
        } catch (ClassNotFoundException e) {}

        res[i] = propDesc;
      }

      return res;
    } catch (IntrospectionException e) {
      System.err.println("PreferencesBeanInfo: " + e);
      res = null;
    }
    return res;
  }
}



