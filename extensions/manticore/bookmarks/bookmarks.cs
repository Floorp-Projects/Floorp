/* -*- Mode: C#; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
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
 * Silverstone Interactive.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com>
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

namespace Silverstone.Manticore.Bookmarks
{
  using System;

  using System.Xml;
  using System.IO;

  using System.Collections;

  using Silverstone.Manticore.Core;
  using Silverstone.Manticore.App;
  
  public class Bookmarks : IDataStore
  {
    private XmlDocument mBookmarksDocument;
    private String mBookmarksFile;
    private Queue mObservers = null;
    private bool mBookmarksLoaded = false;

    ///////////////////////////////////////////////////////////////////////////
    // Bookmarks implementation

    public Bookmarks()
    {
      mBookmarksDocument = new XmlDocument();
    }

    ~Bookmarks()
    {
      mBookmarksDocument.Save(mBookmarksFile);
    }

    public void LoadBookmarks()
    {
      // First of all, the user may have chosen to place the bookmark file
      // in a specific location.
      mBookmarksFile = ServiceManager.Preferences.GetStringPref("browser.bookmarksfile");

      // Otherwise, look in the profile directory.
      if (mBookmarksFile == "")
        mBookmarksFile = FileLocator.GetManticorePath("LocalBookmarks");

      // Support remote bookmarks by setting up an asynchronous load & parse. 
      try 
      {
        mBookmarksDocument.Load(mBookmarksFile);
      }
      catch (XmlException) 
      {
        // Something went wrong, we'll just assume a malformed or non-existant 
        // preferences file, blow it away and insert a new one. Could potentially
        // be dangerous. 
        try 
        {
          File.Copy(@"defaults\bookmarks.xml", mBookmarksFile, true);
        }
        catch (DirectoryNotFoundException) 
        {
          String manticoreAppData = FileLocator.GetManticorePath("AppData");
          Directory.CreateDirectory(manticoreAppData);
          File.Copy(@"defaults\bookmarks.xml", mBookmarksFile, true);
        }
      }
      mBookmarksLoaded = true;
    }

    public void ImportBookmarks()
    {
      // Import a Netscape bookmarks file.
    }

    public string ResolveKeyword(string aURL)
    {
      // XXX implement me
      return "";
    }

    public String CreateBookmark(String aLabel, String aParentID, int aPosition)
    {
      XmlElement parentElt = GetElementById(aParentID);

      XmlElement childElt = mBookmarksDocument.CreateElement("item");
      childElt.SetAttribute("label", aLabel);
      String bookmarkID = Bookmarks.GenerateID();
      childElt.SetAttribute("id", bookmarkID);
      if (aPosition != -1)
        parentElt.InsertBefore(childElt, parentElt.ChildNodes[aPosition]);
      else
        parentElt.AppendChild(childElt);

      CommandTarget target = new CommandTarget(aLabel, "", bookmarkID, false);
      CommandTarget parentTarget = new CommandTarget("", "", aParentID, true);
      IEnumerator observers = mObservers.GetEnumerator();
      while (observers.MoveNext()) 
      {
        IDataStoreObserver idso = observers.Current as IDataStoreObserver;
        idso.OnNodeAdded(target, parentTarget, -1);
      }
      
      return bookmarkID;
    }

    public void DeleteBookmark(Bookmark aBookmark)
    {

    }

    /// <summary>
    /// Retrieve the string value of a given Bookmark property. We expose this API
    /// so as not to have to expose the internal DOM implementation of Bookmarks. 
    /// </summary>
    /// <param name="aBookmarkID"></param>
    /// <param name="aAttribute"></param>
    /// <returns></returns>
    public String GetBookmarkAttribute(String aBookmarkID, String aAttribute)
    {
      XmlElement elt = GetElementById(aBookmarkID);
      return elt.GetAttribute(aAttribute);
    }

    public void SetBookmarkAttribute(String aBookmarkID, String aAttribute, String aValue)
    {
      XmlElement elt = GetElementById(aBookmarkID);

      CommandTarget oldTarget = new CommandTarget();
      switch (aAttribute) 
      {
        case "label":
          oldTarget.Label = elt.GetAttribute(aAttribute);
          break;
        case "icon":
          oldTarget.IconURL = elt.GetAttribute(aAttribute);
          break;
        case "container":
          oldTarget.IsContainer = elt.GetAttribute(aAttribute) == "true";
          break;
        case "open":
          oldTarget.IsOpen = elt.GetAttribute(aAttribute) == "true";
          break;
      }
      oldTarget.Data = aBookmarkID;

      elt.SetAttribute(aAttribute, aValue);

      // Enumerate the attributes presentation cares about and set the attribute on the
      // target if it is being changed. 
      // XXX could optimize this. 
      CommandTarget newTarget = new CommandTarget();
      switch (aAttribute) 
      {
        case "label":
          newTarget.Label = aValue;
          break;
        case "icon":
          newTarget.IconURL = aValue;
          break;
        case "container":
          newTarget.IsContainer = aValue == "true";
          break;
        case "open":
          newTarget.IsOpen = aValue == "true";
          break;
      }
      newTarget.Data = aBookmarkID;

      IEnumerator observers = mObservers.GetEnumerator();
      while (observers.MoveNext()) 
      {
        IDataStoreObserver idso = observers.Current as IDataStoreObserver;
        idso.OnNodeChanged(oldTarget, newTarget);
      }
    }

    public static String GenerateID()
    {
      // Generate a random ID for a bookmark item
      Random rand = new Random();
      return String.Format("NC:Bookmark${0:X}", rand.Next());
    }

    ///////////////////////////////////////////////////////////////////////////
    // IDataStore implementation

    public void GetElements(String aContainerID, out IEnumerator aElements)
    {
      XmlElement container = GetElementById(aContainerID);
      if (container == null)
        container = mBookmarksDocument.DocumentElement.FirstChild as XmlElement;
      int itemCount = container.ChildNodes.Count;
      Queue items = new Queue();
      for (int i = 0; i < itemCount; ++i) 
      {
        XmlElement currElt = container.ChildNodes[i] as XmlElement;
        // If the bookmark does not have an ID, generate one and set it. 
        if (!currElt.HasAttribute("id") || currElt.GetAttribute("id") == "")
          currElt.SetAttribute("id", Bookmarks.GenerateID());
        
        CommandTarget target = new CommandTarget();
        target.Label = currElt.GetAttribute("label");
        target.AccessKey = currElt.GetAttribute("accesskey");
        target.Data = currElt.GetAttribute("id");
        target.IsContainer = currElt.HasChildNodes;
        target.IconURL = currElt.GetAttribute("icon");
        target.IsOpen = currElt.GetAttribute("open") == "true";
        
        items.Enqueue(target);
      }
      aElements = items.GetEnumerator();
    }

    public void AddObserver(IDataStoreObserver aObserver)
    {
      if (mObservers == null)
        mObservers = new Queue();
      mObservers.Enqueue(aObserver);
    }

    /// <summary>
    /// XXX This is a hack, is really bad and inefficient and should go away when we figure
    /// out how to make XmlDocument.GetElementById to work. 
    /// </summary>
    /// <param name="aID"></param>
    /// <returns></returns>
    public XmlElement GetElementById(String aID)
    {
      XmlNodeList list = mBookmarksDocument.GetElementsByTagName("item");
      int count = list.Count;
      for (int i = 0; i < count; ++i) 
      {
        XmlElement elt = list[i] as XmlElement;
        if (elt != null) 
        {
          if (elt.GetAttribute("id") == aID)
            return elt;
        }
      }
      return null;
    }
  }

  public class Bookmark
  {
    public String Name;
    public String URL;
    public String ID;
    public String Keyword;
    public String Description;
    public String IconURL;
    public BookmarkType Type;
  }

  public enum BookmarkType
  {
    Bookmark,
    Folder,
    Separator,
    IEFavoriteFolder,
    IEFavorite,
    FileSystemObject,
    PersonalToolbarFolder
  }
}

