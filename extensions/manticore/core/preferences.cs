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

namespace Silverstone.Manticore.Core
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

    public void InitializeDefaults()
    {
      // Do we ever want to support multiple defaults files? For now, no.
      // XXX need a better place for this file.
      ReadDocument(@"defaults\default-prefs.xml", mDefaultsDocument);
    }

    private void ReadDocument(String aFile, XmlDocument aDocument)
    {
      XmlTextReader reader = new XmlTextReader(aFile);
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();
      String xml = reader.ReadOuterXml();
      reader.Close();
      aDocument.LoadXml(xml);
    }

    // Called once at Startup
    public void LoadPreferencesFile(String aFile)
    {
      ReadDocument(aFile, mPrefsDocument);

      mPrefsDocument.NodeChanged += new XmlNodeChangedEventHandler(OnNodeChanged);
      mPrefsDocument.NodeRemoved += new XmlNodeChangedEventHandler(OnNodeRemoved);
      mPrefsDocument.NodeInserted += new XmlNodeChangedEventHandler(OnNodeInserted);
    }

    public void LoadUserPreferences()
    {
      String manticoreAppData = FileLocator.GetManticorePath("AppData");
      String prefPath = FileLocator.GetManticorePath("UserPrefs");
      try {
        LoadPreferencesFile(prefPath);
      }
      catch (XmlException) {
        // Something went wrong, we'll just assume a malformed or non-existant 
        // preferences file, blow it away and insert a new one. Could potentially
        // be dangerous. 
        try {
          File.Copy(@"defaults\user-prefs.xml", prefPath, true);
        }
        catch (DirectoryNotFoundException) {
          Directory.CreateDirectory(manticoreAppData);
          File.Copy(@"defaults\user-prefs.xml", prefPath, true);
        }
        LoadPreferencesFile(prefPath);
      }
    }

    public void FlushUserPreferences()
    {
      FlushPreferencesFile(FileLocator.GetManticorePath("UserPrefs"));
    }

    public void OnNodeChanged(object sender, XmlNodeChangedEventArgs e)
    {
    }

    public void OnNodeRemoved(object sender, XmlNodeChangedEventArgs e)
    {
    }

    public void OnNodeInserted(object sender, XmlNodeChangedEventArgs e)
    {
    }

    public void FlushPreferencesFile(String aFile)
    {
      UTF8Encoding enc = new UTF8Encoding();
      XmlTextWriter writer = new XmlTextWriter(aFile, enc);
      writer.Formatting = Formatting.Indented;
      mPrefsDocument.WriteTo(writer);
      writer.Flush();
    }
 
    //
    // The Manticore preferences file takes the following (XML) format:
    // 
    // <preferences>
    //   <foopy>
    //     <noopy type="int" value="42">
    //       <noo type="bool" value="true"/>
    //       <goo type="string" value="goats"/>
    //     </noopy>
    //   </foopy>
    // </preferences>
    //
    // where this maps to preferences called:
    // foopy.noopy (int pref, value 42)
    // foopy.noopy.noo (bool pref, value true);
    // foopy.noopy.goo (string pref, value "goats");
    //
    private XmlElement CreateBranch(String aPrefName) 
    {
      String[] names = aPrefName.Split('.');
      XmlElement elt = mPrefsDocument.DocumentElement;
      for (int i = 0; i < names.Length; ++i)
        elt = CreateBranch(names[i], elt);
      return elt;
    }

    private XmlElement CreateBranch(String aBranchName, XmlElement aRoot) 
    {
      XmlElement elt = GetBranch(aBranchName, aRoot);
      if (elt == null) {
        elt = mPrefsDocument.CreateElement(aBranchName);
        aRoot.AppendChild(elt);
      }
      return elt;
    }

    private XmlElement GetBranchElement(String aBranchName) 
    {
      String[] names = aBranchName.Split('.');
      XmlElement elt = mPrefsDocument.DocumentElement;
      for (int i = 0; i < names.Length && elt != null; ++i)
        elt = GetBranch(names[i], elt);
      
      // The preference wasn't found in the user preferences
      // file, look in the default preferences file. 
      if (elt == null) {
        elt = mDefaultsDocument.DocumentElement;
        for (int i = 0; i < names.Length; ++i)
          elt = GetBranch(names[i], elt);
      }

      return elt;
    }

    private XmlElement GetBranch(String aBranchName, XmlElement aRoot) 
    {
      // First, check to see if the specified root already has a branch
      // specified. If it exists, hand that root back. 
      int childCount = aRoot.ChildNodes.Count;
      for (int i = 0; i < childCount; ++i) {
        if (aRoot.ChildNodes[i].LocalName == aBranchName)
          return aRoot.ChildNodes[i] as XmlElement;
      }
      return null;
    }

    public PrefBranch GetBranch(String aBranchName)
    {
      return new PrefBranch(aBranchName, GetBranchElement(aBranchName));
    }

    public void RemoveBranch(String aBranchName) 
    {
      XmlElement elt = GetBranchElement(aBranchName);
      XmlNode parent = elt.ParentNode;
      parent.RemoveChild(elt);
      while (parent != null && 
             parent != (mPrefsDocument.DocumentElement as XmlNode)) {
        if (!parent.HasChildNodes) {
          parent.ParentNode.RemoveChild(parent);
          parent = parent.ParentNode;
        }
        else 
          break;
      }
    }

    public bool GetBoolPref(String aPrefName)
    {
      XmlElement elt = GetBranchElement(aPrefName);
      return elt != null ? elt.GetAttribute("value") == "true" : false;
    }

    public void SetBoolPref(String aPrefName, bool aPrefValue)
    {
      XmlElement childElt = CreateBranch(aPrefName);
      if (childElt != null) 
        childElt.SetAttribute("value", aPrefValue ? "true" : "false");
    }

    public void RemovePref(String aPrefName)
    {
      RemoveBranch(aPrefName);
    }

    public int GetIntPref(String aPrefName)
    {
      XmlElement elt = GetBranchElement(aPrefName);
      return elt != null ? Int32.Parse(elt.GetAttribute("value")) : 0;
    }

    public void SetIntPref(String aPrefName, int aPrefValue)
    {
      XmlElement elt = CreateBranch(aPrefName);
      if (elt != null) {
        Object o = aPrefValue;
        elt.SetAttribute("value", o.ToString());
      }
    }

    public String GetStringPref(String aPrefName)
    {
      XmlElement elt = GetBranchElement(aPrefName);
      return elt != null ? elt.GetAttribute("value") : "";
    }

    public void SetStringPref(String aPrefName, String aPrefValue)
    {
      XmlElement elt = CreateBranch(aPrefName);
      if (elt != null)
        elt.SetAttribute("value", aPrefValue);
    }

    public static String ResolvePref(XmlElement aElement)
    {
      String rv = aElement.LocalName;
      XmlElement temp = aElement;
      while (true) {
        temp = temp.ParentNode as XmlElement;
        if (temp == null || temp.LocalName == "preferences")
          break;
        rv = temp.LocalName + "." + rv;
      }
      return rv;
    }
  }

  public class PrefBranch : IEnumerator
  {
    //private XmlElement mRoot;
    private String mBranchName;
    private XmlElement mRoot;
    private XmlElement mCurrent = null;

    public PrefBranch(String aPrefBranchName, XmlElement aBranchRoot)
    {
      mBranchName = aPrefBranchName;
      mRoot = aBranchRoot;
    }

    public Object Current
    {
      get 
      {
        return Preferences.ResolvePref(mCurrent) as Object; 
      }
    }

    public bool MoveNext()
    {
      if (mCurrent != null) 
      {
        if (mCurrent.NextSibling != null) 
          mCurrent = mCurrent.NextSibling as XmlElement;
        else 
          return false;
      }
      else 
        mCurrent = mRoot.FirstChild as XmlElement;
      return true;
    }

    public void Reset()
    {
      mCurrent = mRoot.FirstChild as XmlElement;
    }
  }
}