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

namespace Silverstone.Manticore.Bookmarks
{
  using System;

  using System.Xml;
  using System.IO;

  using Silverstone.Manticore.App;

  public class Bookmarks
  {
    private XmlDocument mBookmarksDoc;
    private String mBookmarksFile;
    private ManticoreApp mApplication;

    public Bookmarks(ManticoreApp aApplication)
    {
      mApplication = aApplication;

      mBookmarksDoc = new XmlDocument();
      mBookmarksFile = mApplication.Prefs.GetStringPref("browser.bookmarksfile");
      // XXX Need to do windows profile stuff here
      if (mBookmarksFile == "") 
        mBookmarksFile = "bookmarks.xml";
    }

    public void LoadBookmarks()
    {
      // Support remote bookmarks by setting up an asynchronous load & parse. 
      XmlTextReader reader = new XmlTextReader(mBookmarksFile);
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();
      String xml = reader.ReadOuterXml();
      reader.Close();
      mBookmarksDoc.LoadXml(xml);
    }

    public void ImportBookmarks()
    {
      // Import a Netscape bookmarks file.
    }

    public void CreateBookmark(Bookmark aBookmark, Bookmark aBookmarkFolder)
    {
      
    }

    public void DeleteBookmark(Bookmark aBookmark)
    {

    }

    public String GenerateID()
    {
      // Generate a random ID for a bookmark item
      return "";
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

