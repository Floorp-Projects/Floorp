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

  public abstract class DynamicTreeBuilder : ContentBuilder, IDataStoreObserver
  {
    /// <summary>
    /// The |TreeNode| that is the parent of the content builder
    /// </summary>
    protected TreeNode mParent = null;

    protected TreeNodeCollection mParentNodes = null;
    
    /// <summary>
    /// |mBefore| and |mAfter| define the static boundaries within which
    /// the DynamicTreeBuilder operates. Used if the builder builds items
    /// which are siblings to static |TreeNode|s.
    /// </summary>
    protected TreeNode mBefore = null;
    protected TreeNode mAfter = null;

    /// <summary>
    /// The containing |TreeView|
    /// </summary>
    protected ManticoreTreeView mTreeView = null;

    /// <summary>
    /// All of this builder's |TreeNode|s, keyed by DataStore id. 
    /// </summary>
    protected Hashtable mNodes = null;
    

    /// <summary>
    /// This is the default point of insertion of new nodes. Initially, this
    /// is the index of |mBefore|, or 0 if |mBefore| is null. As items are added,
    /// it is incremented. 
    /// </summary>
    protected int mInsertionPoint = 0;
    
    public DynamicTreeBuilder(ManticoreTreeView aTreeView, TreeNode aParent)
    {
      if (aTreeView == null) 
        throw new Exception();

      if (aParent != null) 
      {
        mParent = aParent;
        mParentNodes = aParent.Nodes;
      }
      else 
        mParentNodes = aTreeView.Nodes;

      mTreeView = aTreeView;
      
      InitializeComponent();
    }
    
    public DynamicTreeBuilder(ManticoreTreeView aTreeView, TreeNode aParent, TreeNode aBefore, TreeNode aAfter)
    {
      mParent = aParent;
      mTreeView = aTreeView;
      
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
        mInsertionPoint = mParentNodes.IndexOf(mBefore) + 1;
      }
      else
        mInsertionPoint = 0;
    }
    
    protected virtual void InitializeComponent()
    {
      mNodes = new Hashtable();
      
      // XXX for now, just be stupid and generate everything when the root
      //     menu is opened. 
      mTreeView.BeforeExpand += new TreeViewCancelEventHandler(OnBeforeExpand);
    }

    /// <summary>
    /// Overridden by clients to provide command handling for built menus. 
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    protected virtual void OnBeforeExpand(Object sender, TreeViewCancelEventArgs e)
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
      
      int itemKey = newTarget.Data.GetHashCode();
      if (mNodes.ContainsKey(itemKey)) 
      {
        ManticoreTreeNode item = mNodes[itemKey] as ManticoreTreeNode;
        // Check for Label/AccessKey change. 
        if (newTarget.Label != oldTarget.Label) 
          item.Text = newTarget.Label;
        if (newTarget.IsContainer) 
          item.ImageIndex = mTreeView.GetIconIndex(newTarget);
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

      // Determine based on conditions defined by the |TreeView|
      // whether or not this node should be built. 
      if (!mTreeView.ShouldBuild(childTarget))
        return;

      int childKey = childTarget.Data.GetHashCode();
      if (!mNodes.ContainsKey(childKey)) 
      {
        int parentKey = parentTarget.Data.GetHashCode();

        ManticoreTreeNode parentNode;
        if (mNodes.ContainsKey(parentKey))
          parentNode = mNodes[parentKey] as ManticoreTreeNode;
        else
          parentNode = mParent as ManticoreTreeNode;
        
        if (parentNode != null) 
        {
          ManticoreTreeNode childNode = new ManticoreTreeNode(childTarget.Label,
            childTarget.Data);
          if (parentNode != null) 
          {
            int imageIndex = mTreeView.GetIconIndex(childTarget);
            if (imageIndex > -1) 
              childNode.ImageIndex = imageIndex;

            parentNode.Nodes.Insert(aIndex, childNode);
            if (childTarget.IsContainer && childTarget.IsOpen)
              childNode.Expand();
            mNodes.Add(childKey, childNode);
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
      if (mNodes.ContainsKey(childKey)) 
      {
        ManticoreTreeNode childNode = mNodes[childKey] as ManticoreTreeNode;
        ManticoreTreeNode parentNode = childNode.Parent as ManticoreTreeNode;
        if (parentNode != null)
          parentNode.Nodes.Remove(childNode);
        else
          mTreeView.Nodes.Remove(childNode);
        
        mNodes.Remove(childKey);
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
      Recurse(mRoot, mParentNodes);
    }

    public void Recurse(String aRoot, TreeNodeCollection aNodes)
    {
      IEnumerator items;
      mDataStore.GetElements(aRoot, out items);
      items.Reset();
      
      ManticoreTreeNode node;
      
      while (items.MoveNext()) 
      {
        // |id| is the item's unique identifier within the DataStore. 
        // The handling code can use this to probe for more information
        // about the selected item. 
        CommandTarget current = items.Current as CommandTarget;
        if (current != null) 
        {
          // Determine based on conditions defined by the |TreeView|
          // whether or not this node should be built. 
          if (!mTreeView.ShouldBuild(current))
            continue;

          String id = current.Data as String; 
         
          int idKey = id.GetHashCode();
          
          if (!mNodes.ContainsKey(idKey)) 
          {
            node = new ManticoreTreeNode(current.Label, id);

            int imageIndex = mTreeView.GetIconIndex(current);
            if (imageIndex > -1) 
              node.ImageIndex = imageIndex;
            aNodes.Insert(mInsertionPoint++, node);
            if (current.IsContainer && current.IsOpen)
              node.Expand();
            mNodes.Add(id.GetHashCode(), node);

            // If we're a container, recurse 
            if (current.IsContainer)
              Recurse(id, node.Nodes);
          }
        }
      }
      
      ResetInsertionPoint();
    }

    ///////////////////////////////////////////////////////////////////////////
    //

    public TreeNode Parent
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

    public TreeNode Before
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

    public TreeNode After 
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
  
  public class BaseTreeBuilder : DynamicTreeBuilder
  {
    public BaseTreeBuilder(ManticoreTreeView aTreeView, TreeNode aParent) : base(aTreeView, aParent)
    {
      InitializeComponent();
    }
    
    public BaseTreeBuilder(ManticoreTreeView aTreeView, 
                           TreeNode aParent, 
                           TreeNode aBefore, 
                           TreeNode aAfter) : base(aTreeView, aParent, aBefore, aAfter)
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
