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
 *  Ben Goodger <ben@netscape.com> (Original Author)
 *
 */
 
namespace Silverstone.Manticore.Bookmarks
{
  using System;
  using System.Drawing;
  using System.Windows.Forms;
  using System.Collections;
  
  using Silverstone.Manticore.Core;
  using Silverstone.Manticore.Toolkit;
  
  /// <summary>
	/// A Bookmarks Tree widget
	/// </summary>
  public class BookmarksTreeView : ManticoreTreeView
  {
    protected BaseTreeBuilder mBuilder;
    protected Queue mFilterAttributes;

    public BookmarksTreeView(String aRoot)
    {
      mBuilder = new BaseTreeBuilder(this, null);
      mBuilder.Root = aRoot;
      mBuilder.DataStore = DataStoreRegistry.GetDataStore("Bookmarks");
      mBuilder.DataStore.AddObserver(mBuilder);

      // Use bright green as transparent colour
      mIconTransparentColor = ColorTranslator.FromOle(0x00FF00);

      ImageList = new ImageList();
      // Built in Bookmark icons
      ImageList.Images.Add(Image.FromFile(@"resources\bookmark-folder-closed.png"), mIconTransparentColor);
      ImageList.Images.Add(Image.FromFile(@"resources\bookmark-folder-open.png"), mIconTransparentColor);
      ImageList.Images.Add(Image.FromFile(@"resources\bookmark.png"), mIconTransparentColor);

      // Don't draw lines to root
      ShowRootLines = false;
      ShowPlusMinus = true;

      AfterLabelEdit += new NodeLabelEditEventHandler(OnAfterLabelEdit);
      AfterExpand += new TreeViewEventHandler(OnAfterExpand);
      AfterCollapse += new TreeViewEventHandler(OnAfterCollapse);
    }

    public void Build()
    {
      mBuilder.Build();
    }

    public String Root 
    {
      get 
      {
        return mBuilder.Root;
      }
      set 
      {
        if (mBuilder.Root != value)
          mBuilder.Root = value;
      }
    }

    public void AddCriteria(String[] aAttrValuePair)
    {
      if (mFilterAttributes == null)
        mFilterAttributes = new Queue();
      mFilterAttributes.Enqueue(aAttrValuePair);
    }

    public override bool ShouldBuild(CommandTarget aTarget)
    {
      Bookmarks bmks = ServiceManager.Bookmarks;

      if (mFilterAttributes != null) 
      {
        IEnumerator criteria = mFilterAttributes.GetEnumerator();
        while (criteria.MoveNext()) 
        {
          String[] singleCriteria = criteria.Current as String[];
            if (bmks.GetBookmarkAttribute(aTarget.Data as String, singleCriteria[0]) != singleCriteria[1])
              return false;
        }
      }
      return true;
    }

    public override int GetIconIndex(CommandTarget aTarget)
    {
      int index = 2;
      if (aTarget.IsContainer) 
      {
        index = 0;
        if (aTarget.IsOpen) 
          index = 1;
      }

      int fileIndex = GetIconIndex(aTarget.IconURL);
      if (fileIndex != -1)
        index = fileIndex;
      return index;
    }

    /// <summary>
    /// Retrieves the single root |ManticoreTreeNode| in the TreeView.
    /// </summary>
    /// <returns></returns>
    protected ManticoreTreeNode GetRootItem()
    {
      if (Nodes.Count == 0) 
        return null;
      return Nodes[0] as ManticoreTreeNode;
    }

    public void NewFolder()
    {
      ManticoreTreeNode root = GetRootItem();
      if (root != null) 
      {
        ManticoreTreeNode temp = new ManticoreTreeNode("New Folder", null);
        root.Nodes.Add(temp);
        LabelEdit = true;
        temp.BeginEdit();
      }
    }

    protected void OnAfterLabelEdit(Object sender, NodeLabelEditEventArgs e)
    {
      ManticoreTreeNode root = GetRootItem();
      if (root != null && e.Label != "") 
      {
        ManticoreTreeNode temp = e.Node as ManticoreTreeNode;
        String parentID = root.Data as String;
        Bookmarks bmks = ServiceManager.Bookmarks;
        String bookmarkID = bmks.CreateBookmark(e.Label, parentID, -1);
        bmks.SetBookmarkAttribute(bookmarkID, "container", "true");
        bmks.SetBookmarkAttribute(bookmarkID, "open", "true");
        e.Node.Text = "FruitLoop";
        e.Node.EndEdit(false);
        root.Nodes.Remove(temp);
        LabelEdit = false;
      }
      else 
        e.Node.EndEdit(true);
    }

    protected void OnAfterExpand(Object sender, TreeViewEventArgs e)
    {
      ManticoreTreeNode node = e.Node as ManticoreTreeNode;
      String nodeID = node.Data as String;
      Bookmarks bmks = ServiceManager.Bookmarks;
      bmks.SetBookmarkAttribute(nodeID, "open", "true");
      node.ImageIndex = 1;
    }

    protected void OnAfterCollapse(Object sender, TreeViewEventArgs e)
    {
      ManticoreTreeNode node = e.Node as ManticoreTreeNode;
      String nodeID = node.Data as String;
      Bookmarks bmks = ServiceManager.Bookmarks;
      bmks.SetBookmarkAttribute(nodeID, "open", "false");
      node.ImageIndex = 0;
    }
	}
}

