/* -*- Mode: C#; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is Manticore.
 * 
 * The Initial Developer of the Original Code is
 * Silverstone Interactive. Portions created by Silverstone Interactive are
 * Copyright (C) 2001 Silverstone Interactive. 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Ben Goodger <ben@netscape.com>
 *
 */

namespace Silverstone.Manticore.Preferences
{
  using System;
  using System.Collections;

  using System.IO;
  using System.Xml;
  using System.Text;

  public class Preferences 
  {
    XmlDocument mDefaultsDocument;
    XmlDocument mPrefsDocument;

    public Preferences()
    {
      mDefaultsDocument = new XmlDocument();
      mPrefsDocument = new XmlDocument();
    }

    public void InitializeDefaults(String aDefaults)
    {
      // Do we ever want to support multiple defaults files? For now, no.
      mDefaultsDocument.Load(aDefaults);
    }

    public void LoadPreferencesFile(String aFile)
    {
      mPrefsDocument.Load(aFile);
    }

    public void FlushPreferencesFile(String aFile)
    {
      UTF8Encoding enc = new UTF8Encoding();
      XmlTextWriter writer = new XmlTextWriter(aFile, enc);
      writer.Formatting = Formatting.Indented;
      mPrefsDocument.WriteTo(writer);
      writer.Flush();
    }

    private XmlElement GetPrefElement(String aPrefType, String aPrefName)
    {
      return GetPrefElement(aPrefType, aPrefName, false);
    }

    private XmlElement GetPrefElement(String aPrefType, String aPrefName, 
                                      bool aCreate)
    {
      XmlDocument[] documents = {mPrefsDocument, mDefaultsDocument};
      for (int i = 0; i < documents.Length; ++i) {
        XmlNodeList prefs = documents[i].GetElementsByTagName(aPrefType);
        int prefCount = prefs.Count;
        for (int k = 0; k < prefCount; ++k) {
          XmlNode currentNode = prefs[k];
          if (currentNode is XmlElement) {
            XmlElement elt = currentNode as XmlElement;
            if (elt.GetAttribute("name") == aPrefName)
              return elt;
          }
        }
      }
      if (aCreate) {
        // If an element is not found, create one. 
        XmlElement elt = mPrefsDocument.CreateElement(aPrefType);
        elt.SetAttribute("name", aPrefName);
        elt.SetAttribute("value", "");
        mPrefsDocument.DocumentElement.AppendChild(elt);
      }
      return null;
    }

    public bool GetBoolPref(String aPrefName)
    {
      XmlElement elt = GetPrefElement("boolpref", aPrefName);
      return elt != null ? elt.GetAttribute("value") == "true" : false; 
    }

    public void SetBoolPref(String aPrefName, bool aPrefValue)
    {
      XmlElement elt = GetPrefElement("boolpref", aPrefName, true);
      if (elt != null)
        elt.SetAttribute("value", aPrefValue ? "true" : "false");
    }

    public int GetIntPref(String aPrefName)
    {
      XmlElement elt = GetPrefElement("intpref", aPrefName);
      return elt != null ? Int32.Parse(elt.GetAttribute("value")) : 0;
    }

    public void SetIntPref(String aPrefName, int aPrefValue)
    {
      XmlElement elt = GetPrefElement("intpref", aPrefName, true);
      if (elt != null) {
        Object o = aPrefValue;
        elt.SetAttribute("value", o.ToString());
      }
    }

    public String GetStringPref(String aPrefName)
    {
      XmlElement elt = GetPrefElement("stringpref", aPrefName);
      return elt != null ? elt.GetAttribute("value") : "";
    }

    public void SetStringPref(String aPrefName, String aPrefValue)
    {
      XmlElement elt = GetPrefElement("stringpref", aPrefName, true);
      if (elt != null) 
        elt.SetAttribute("value", aPrefValue);
    }

  }

}