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

namespace Silverstone.Manticore.Toolkit
{
  using System;
  using System.ComponentModel;
  using System.Drawing;
  using System.Windows.Forms;

  using System.Collections;

  using System.IO;
  using System.Xml;

  using Silverstone.Manticore.Core;
  using Silverstone.Manticore.Toolkit;

  public class MenuBuilder
  {
    protected String mMenuFile = "";
    protected Form mForm = null;
    protected MainMenu mMainMenu = null;

    public Hashtable mItems;
    public Hashtable mBuilders;

    public event EventHandler OnCommand;

    public MenuBuilder(String aFile, Form aForm)
    {
      mMenuFile = aFile;
      mForm = aForm;
      mItems = new Hashtable();
      mBuilders = new Hashtable();

      mMainMenu = new MainMenu();
      mForm.Menu = mMainMenu;
    }

    public void Build()
    {
      XmlTextReader reader;
      reader = new XmlTextReader(mMenuFile);
      
      reader.WhitespaceHandling = WhitespaceHandling.None;
      reader.MoveToContent();

      Recurse(reader, mMainMenu);
    }

    protected MenuItem mCurrentMenuItem;
    protected void Recurse(XmlTextReader reader, Menu root)
    {
      String inner = reader.ReadInnerXml();
    
      NameTable nt = new NameTable();
      XmlNamespaceManager nsmgr = new XmlNamespaceManager(nt);
      XmlParserContext ctxt = new XmlParserContext(null, nsmgr, null, XmlSpace.None);
      XmlTextReader reader2 = new XmlTextReader(inner, XmlNodeType.Element, ctxt);

      while (reader2.Read()) 
      {
        if (reader2.NodeType == XmlNodeType.Element) 
        {
          switch (reader2.LocalName) 
          {
            case "menu":
              // Menuitem. Find the name, accesskey, command and id strings
              String[] values = new String[3] {"", "", ""};
              String[] names = new String[3] {"label", "accesskey", "command"};
              for (int i = 0; i < names.Length; ++i) 
              {
                if (reader2.MoveToAttribute(names[i]) &&
                  reader2.ReadAttributeValue())
                  values[i] = reader2.Value; // XXX need to handle entities
                reader2.MoveToElement();
              }

              // Handle Accesskey
              values[0] = ManticoreMenuItem.GenerateAccessKeyString(values[0], values[1]);

              // Create menu item and attach an event handler
              // BLUESKY - should we support data stored in the XML file as an attribute?
              mCurrentMenuItem = new ManticoreMenuItem(values[0], 
                                                       new EventHandler(OnCommandInternal),
                                                       values[2], "");
              if (values[2] != "")
                mItems.Add(values[2], mCurrentMenuItem);
              root.MenuItems.Add(mCurrentMenuItem);
              Recurse(reader2, mCurrentMenuItem);
              break;
            case "menuseparator":
              mCurrentMenuItem = new MenuItem("-");
              root.MenuItems.Add(mCurrentMenuItem);
              break;
            case "menubuilder":
              String id = "";
              if (reader2.MoveToAttribute("id") &&
                  reader2.ReadAttributeValue())
                id = reader2.Value;
              reader2.MoveToElement();
              String datastore = "";
              if (reader2.MoveToAttribute("datastore") && 
                  reader2.ReadAttributeValue())
                datastore = reader2.Value;

              BaseMenuBuilder builder = new BaseMenuBuilder(mMainMenu, root as MenuItem, mCurrentMenuItem, null);
              builder.Root = id;
              builder.DataStore = DataStoreRegistry.GetDataStore(datastore);
              builder.DataStore.AddObserver(builder);
              builder.OnCommand += new EventHandler(OnCommandInternal);
              mBuilders.Add(builder.GetHashCode(), builder);
              break;
          }
        }
      }
    }

    protected void OnCommandInternal(Object sender, EventArgs e)
    {
      if (OnCommand != null) 
        OnCommand(sender, e);
    }
  }

  // XXX should be "ManticoreMenuItem"
  public class ManticoreMenuItem : MenuItem
  {
    private String mCommand;
    /// <summary>
    /// The Command String associated with this item. Used with |IController|
    /// </summary>
    public String Command
    {
      get
      {
        return mCommand;
      }
    }

    /// <summary>
    /// Any data associated with this item. Useful to hold DataStore ID references.
    /// </summary>
    private Object mData;
    public Object Data 
    {
      get 
      {
        return mData;
      }
    }

    public static String GenerateAccessKeyString(String aLabel, String aAccessKey)
    {
      int idx = aLabel.ToLower().IndexOf(aAccessKey.ToLower());
      if (idx != -1)
        return aLabel.Insert(idx, "&");
      else 
        return aLabel + " (&" + aAccessKey.ToUpper() + ")";
    }

    public ManticoreMenuItem(String aLabel, EventHandler aHandler, String aCommand, Object aData) : base(aLabel, aHandler)
    {
      mCommand = aCommand;
      mData = aData;
    }
  }

  public class ManticoreTreeNode : TreeNode
  {
    private Object mData;
    public Object Data
    {
      get 
      {
        return mData;
      }
    }

    public ManticoreTreeNode(String aLabel, Object aData) : base(aLabel)
    {
      mData = aData;
    }
  }

  public class ManticoreTreeView : TreeView
  {
    /// <summary>
    /// Holds imageURL->imageIndex mapping of images
    /// </summary>
    protected Hashtable mImages = null;

    /// <summary>
    /// Number of images currently hashed (current image index)
    /// </summary>
    protected int mImageCount = 0;

    /// <summary>
    /// Transparent colour used in icons. We could be pedantic and allow this
    /// to come from the datastore, but this'll do for now. 
    /// </summary>
    protected Color mIconTransparentColor;

    public virtual bool ShouldBuild(CommandTarget aTarget)
    {
      return true;
    }

    public int GetIconIndex(String aIconURL)
    {
      if (aIconURL == "")
        return -1;

      if (mImages == null)
        mImages = new Hashtable();

      int key = aIconURL.GetHashCode();
      if (!mImages.ContainsKey(key)) 
      {
        if (ImageList == null) 
          ImageList = new ImageList();
        Console.WriteLine(mIconTransparentColor);
        try 
        {
          ImageList.Images.Add(Image.FromFile(aIconURL), mIconTransparentColor);
          mImages.Add(key, mImageCount);
        }
        catch (FileNotFoundException)
        {
          // If the file can't be found, don't add it to the list.
          return -1;
        }
        return mImageCount++;
      }
      else
        return (int) mImages[key];
    }

    /// <summary>
    /// Client overrides to provide special Icons for the particular treeview. 
    /// </summary>
    /// <param name="aCommandTarget"></param>
    /// <returns></returns>
    public virtual int GetIconIndex(CommandTarget aCommandTarget)
    {
      return -1;
    }
  }

}


