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
 *   Ben Goodger <ben@netscape.com> (Original Author)
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
 
namespace Silverstone.Manticore.Toolkit
{
  using System;
  using System.Windows.Forms;
  using System.Collections;

  using Silverstone.Manticore.Core;

  public abstract class DynamicMenuBuilder : ContentBuilder, IDataStoreObserver
  {
    /// <summary>
    /// The |MenuItem| that is the parent of the content builder
    /// </summary>
    protected MenuItem mParent = null;

    protected Menu.MenuItemCollection mParentItems = null;
    
    /// <summary>
    /// |mBefore| and |mAfter| define the static boundaries within which
    /// the DynamicMenuBuilder operates. Used if the builder builds items
    /// which are siblings to static |MenuItem|s.
    /// </summary>
    protected MenuItem mBefore = null;
    protected MenuItem mAfter = null;
    
    /// <summary>
    /// All of this builder's |MenuItem|s, keyed by DataStore id. 
    /// </summary>
    protected Hashtable mMenus = null;

    /// <summary>
    /// This is the default point of insertion of new menus. Initially, this
    /// is the index of |mBefore|, or 0 if |mBefore| is null. As items are added,
    /// it is incremented. 
    /// </summary>
    protected int mInsertionPoint = 0;
    
    public DynamicMenuBuilder(MainMenu aMainMenu, MenuItem aParent)
    {
      if (aParent != null) 
      {
        mParent = aParent;
        mParentItems = aParent.MenuItems;
      }
      else
        mParentItems = aMainMenu.MenuItems;
      
      InitializeComponent();
    }
    
    public DynamicMenuBuilder(MainMenu aMainMenu, MenuItem aParent, MenuItem aBefore, MenuItem aAfter)
    {
      if (aParent != null) 
      {
        mParent = aParent;
        mParentItems = aParent.MenuItems;
      }
      else
        mParentItems = aMainMenu.MenuItems;
      
      
      // XXX we do jack shit to handle this at the moment. TBD. 
      mBefore = aBefore;
      mAfter = aAfter;
      
      ResetInsertionPoint();
    
      InitializeComponent();
    }
    
    private void ResetInsertionPoint()
    {
      if (mBefore != null) 
      {
        // The insertion point is the position immediately after |mBefore|
        mInsertionPoint = mParentItems.IndexOf(mBefore) + 1;
      }
      else
        mInsertionPoint = 0;
    }
    
    protected virtual void InitializeComponent()
    {
      mMenus = new Hashtable();
      
      // XXX for now, just be stupid and generate everything when the root
      //     menu is opened. 
      mParent.Popup += new EventHandler(OnParentPopup);
    }
    
    protected void OnParentPopup(Object sender, EventArgs e)
    {
      // XXX make this smarter. 
      Build();
    }

    ///////////////////////////////////////////////////////////////////////////
    // ContentBuilder Implementation

    public void OnNodeChanging(Object aOldNode, Object aNewNode)
    {
      // Not Implemented.
    }
    public void OnNodeChanged(Object aOldNode, Object aNewNode)
    {
      // Enumerate each of the properties that affect FE presentation, test
      // for difference, and update the FE if necessary.
      
      CommandTarget oldTarget = aOldNode as CommandTarget;
      CommandTarget newTarget = aNewNode as CommandTarget;
      
      if (oldTarget == null && newTarget == null)
        return;
      
      // Check for Label/AccessKey change. 
      if (newTarget.Label != oldTarget.Label || newTarget.AccessKey != oldTarget.AccessKey) 
      {
        int itemKey = newTarget.Data.GetHashCode();
        if (mMenus.ContainsKey(itemKey)) 
        {
          ManticoreMenuItem item = mMenus[itemKey] as ManticoreMenuItem;
          // Re-generate AccessKey and update display Text
          item.Text = ManticoreMenuItem.GenerateAccessKeyString(newTarget.Label, newTarget.AccessKey);;
        }
      }
    }

    public void OnNodeAdding(Object aNewNode, Object aParentNode, int aIndex)
    {
      // Not Implemented. 
    }
    public void OnNodeAdded(Object aChildNode, Object aParentNode, int aIndex)
    {
      // A new Element has been added somewhere. We must find the 
      // parent menu and append it. To interoperate with the Content Builder,
      // the DataStore must provide |CommandTarget|s to this method. 
      CommandTarget childTarget = aChildNode as CommandTarget;
      CommandTarget parentTarget = aParentNode as CommandTarget;
      if (childTarget == null && parentTarget == null)
        return;

      int childKey = childTarget.Data.GetHashCode();
      if (!mMenus.ContainsKey(childKey)) 
      {
        int parentKey = parentTarget.Data.GetHashCode();

        ManticoreMenuItem parentMenu;
        if (mMenus.ContainsKey(parentKey))
          parentMenu = mMenus[parentKey] as ManticoreMenuItem;
        else
          parentMenu = mParent as ManticoreMenuItem;
        
        if (parentMenu != null) 
        {
          String label = childTarget.Label;
          if (childTarget.AccessKey != "")
            label = ManticoreMenuItem.GenerateAccessKeyString(childTarget.Label, 
              childTarget.AccessKey);
          ManticoreMenuItem childMenu = new ManticoreMenuItem(label, 
                                                              new EventHandler(OnCommandInternal), 
                                                              "bookmarks-item", childTarget.Data);
          if (parentMenu != null) 
          {
            int pos = aIndex == -1 ? parentMenu.MenuItems.Count : aIndex;
            parentMenu.MenuItems.Add(pos, childMenu);
            mMenus.Add(childKey, childMenu);
          }
        }
      }
    }

    public void OnNodeRemoving(Object aNodeRemoving)
    {
      // Not Implemented.
    }
    public void OnNodeRemoved(Object aNodeRemoved)
    {
      CommandTarget childTarget = aNodeRemoved as CommandTarget;

      // Remove |MenuItem| representation of |aChildNode|.
      int childKey = childTarget.Data.GetHashCode();
      if (mMenus.ContainsKey(childKey)) 
      {
        ManticoreMenuItem childMenu = mMenus[childKey] as ManticoreMenuItem;
        ManticoreMenuItem parentMenu = childMenu.Parent as ManticoreMenuItem;
        parentMenu.MenuItems.Remove(childMenu);
        mMenus.Remove(childKey);
      }
    }

    /// <summary>
    /// Build content. Builds all content if |LazyState| is set to false.
    /// </summary>
    public override void Build()
    {
      if (mDataStore == null || mRoot == null) 
        throw new Exception();

      // XXX implement acknowledgement of |LazyState|

      Recurse(mRoot, mParentItems);
    }

    public void Recurse(String aRoot, Menu.MenuItemCollection aParentItems)
    {
      IEnumerator items;
      mDataStore.GetElements(aRoot, out items);
      items.Reset();
      
      MenuItem item;
      
      while (items.MoveNext()) 
      {
        // |id| is the item's unique identifier within the DataStore. 
        // The handling code can use this to probe for more information
        // about the selected item. 
        CommandTarget current = items.Current as CommandTarget;
        if (current != null) 
        {
          String id = current.Data as String; 
          
          int idKey = id.GetHashCode();
          
          if (!mMenus.ContainsKey(idKey)) 
          {
            item = new ManticoreMenuItem(current.Label, 
                                         new EventHandler(OnCommandInternal), 
                                         "bookmarks-item", id);
            if (aRoot != Root)
              aParentItems.Add(item);
            else
              aParentItems.Add(mInsertionPoint++, item);
            mMenus.Add(id.GetHashCode(), item);

            if (current.IsContainer)
              Recurse(current.Data as String, item.MenuItems);
          }
        }
      }
      
      ResetInsertionPoint();
    }

    /// <summary>
    /// Clients wishing to observe for |MenuItem| selection will want to listen to
    /// this event. 
    /// </summary>
    public event EventHandler OnCommand;

    /// <summary>
    /// Calls |OnCommand| EventHandler on interested clients indicating a generated
    /// |MenuItem| has been selected.
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    protected void OnCommandInternal(Object sender, EventArgs e)
    {
      if (OnCommand != null) 
        OnCommand(sender, e);
    }

    ///////////////////////////////////////////////////////////////////////////
    //

    public MenuItem Parent
    {
      get 
      {
        return mParent;
      }
      set 
      {
        mParent = value;
      }
    }

    public MenuItem Before
    {
      get 
      {
        return mBefore;
      }
      set 
      {
        mBefore = value;
      }
    }

    public MenuItem After 
    {
      get 
      {
        return mAfter;
      }
      set 
      {
        mAfter = value;
      }
    }
  }
  
  public class BaseMenuBuilder : DynamicMenuBuilder
  {
    public BaseMenuBuilder(MainMenu aMainMenu, 
                           MenuItem aParent) : base(aMainMenu, aParent)
    {
      InitializeComponent();
    }
    
    public BaseMenuBuilder(MainMenu aMainMenu,
                           MenuItem aParent, 
                           MenuItem aBefore, 
                           MenuItem aAfter) : base(aMainMenu, aParent, aBefore, aAfter)
    {
      InitializeComponent();
    }

    protected override void InitializeComponent()
    {
      base.InitializeComponent();

      mDataStore = new SampleDataStore();
      mDataStore.AddObserver(this as IDataStoreObserver);
    } 
  }
}
