/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Will Scullin <scullin@netscape.com>, 21 Aug 1997.
 */

package grendel.widgets;

import java.util.Enumeration;

import javax.swing.Icon;

/**
 * The <code>TreeTableDataModel</code> interface is used to mediate
 * between the underlying data store and the TreeTable. The interface
 * supports two major mechanisms for retrieving tree data: as enumerations
 * or by getting the first child/getting the next sibling of a node.
 * The model only needs to implement one. After initially setting the data
 * model for the <code>TreeTable</code>, the model should notify listeners
 * of changes using the <code>TreeTableModelListener</code> interface.
 *
 * @author Will Scullin
 *
 * @see TreeTable
 * @see TreeTableModelListener
 */

public interface TreeTableDataModel {

  /**
   * Indicates whether the root node is visible. An invisible
   * root node should not have siblings.
   *
   * @return <code>true</code> if the root node should be displayed.
   */

  public boolean showRoot();

  /**
   * Retrieves the children of a given node. If <code>aNode</code>
   * is <code>null</code> then the enumeration returned should represent
   * the nodes at the root level. An implementation that supports this
   * method does not need to implement the <code>getRoot()</code>,
   * <code>getChild()</code> or <code>getNextSibling()</code> methods
   * (they can return <code>null</code>).
   *
   * @return An enumeration of objects representing the children of the
   *          given node.
   *
   * @see #getRoot
   * @see #getChild
   * @see #getNextSibling
   */

  public Enumeration getChildren(Object aNode);

  /**
   * Retrieves the root node of the tree. An implementation that
   * supports this method should implement the <code>getChild()</code> and
   * <code>getNextSibling()</code> methods and does not need to implement
   * the <code>getChildren()</code> method (it can return
   * <code>null</code>).
   *
   * @return An object representing the root of the data model being
   *          displayed.
   *
   * @see #getChildren
   * @see #getChild
   * @see #getNextSibling
   */

  public Object getRoot();

  /**
   * Retrieves the first child of the given node. Should return
   * <code>null</code> if the node has no children. An implementation that
   * supports this method should implement the <code>getRoot()</code> and
   * <code>getNextSibling()</code> methods and does not need to implement
   * the <code>getChildren()</code> method (it can return
   * <code>null</code>).
   *
   * @return An object representing the first child of the given node.
   *
   * @see #getChildren
   * @see #getRoot
   * @see #getNextSibling
   */

  public Object getChild(Object aNode);

  /**
   * Retrieves the next sibling of the given node. Should return
   * <code>null</code> if the node has no next sibling. An implementation
   * thatsupports this method should implement the <code>getChild()</code>
   * and <code>getNextSibling()</code> methods and does not need to
   * implement the <code>getChildren()</code> method (it can return
   * <code>null</code>).
   *
   * @return An object representing the next sibing of the given node.
   *
   * @see #getChildren
   * @see #getRoot
   * @see #getChild
   */

  public Object getNextSibling(Object aNode);

  /**
   * Indicates whether the given node is a leaf. The values
   * returned by <code>isLeaf()</code> and <code>getChild()</code>
   * or <code>getChildren()</code> should be consistant.
   *
   * @return <code>true</code> if the given node is a leaf.
   */

  public boolean isLeaf(Object aNode);

  /**
   * Indicates whether the given node is collapsed.
   *
   * @return <code>true</code> if the given node is collapsed.
   */

  public boolean isCollapsed(TreePath aPath);

  /**
   * Method to set the collapsed state of a node.
   */

  public void setCollapsed(TreePath aPath, boolean aCollapsed);

  /**
   * Retrieves the data for the given node at the column with the given id.
   * The data will be passed on to the <code>CellRenderer</code> of the column
   * for rendering.
   *
   * @return An <code>Object</code> representing the data at the
   * given column for the given node.
   *
   * @see CellRenderer
   * @see Column
   */

  public Object getData(Object aNode, Object aID);

  /**
   * Method for setting the data for the given node at the column with the
   * given id. The data is retrieved from the <code>CellEditor</code> of the
   * column.
   *
   * @see CellEditor
   * @see Column
   */

  public void setData(Object aNode, Object aID, Object aValue);

  /**
   * Retrieves an icon representing the given node. The return value
   * can be <code>null</code> if no icon is to be drawn.
   *
   * @return An <code>Icon</code> representing the given node.
   */

  public Icon getIcon(Object aNode);

  /**
   * Retrieves an icon representing a modified state for a given node.
   * The return value can be <code>null</code> if no overlay icon is to
   * be drawn. The overlay icon should be transparent and the same size
   * as the returned icon.
   *
   * @return An <code>Icon</code> representing the given node state.
   *
   * @see #getIcon
   */

  public Icon getOverlayIcon(Object aNode);

  /**
   * Adds a <code>TreeTableModelListener</code>.
   */

  public void addTreeTableModelListener(TreeTableModelListener aListener);

  /**
   * Removes a <code>TreeTableModelListener</code>.
   */

  public void removeTreeTableModelListener(TreeTableModelListener aListener);
}
