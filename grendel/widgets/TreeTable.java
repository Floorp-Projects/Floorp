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
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 30 Dec 1998
 */

package grendel.widgets;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.Insets;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.awt.event.KeyEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseMotionListener;
import java.awt.event.MouseEvent;

import java.util.Enumeration;
import java.util.Vector;

import javax.swing.event.CellEditorListener;
import javax.swing.CellRendererPane;
import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JViewport;
import javax.swing.KeyStroke;
import javax.swing.Scrollable;
import javax.swing.SwingConstants;
import javax.swing.SwingUtilities;
import javax.swing.UIManager;
import javax.swing.plaf.basic.BasicGraphicsUtils;
import javax.swing.event.ChangeEvent;
import javax.swing.event.ListSelectionEvent;

import grendel.dnd.DropTarget;
import grendel.dnd.DropTargetComponent;
import grendel.dnd.DragSource;

/**
 * A outliner view similar to those used in previous versions of
 * mail and news.
 */

public class TreeTable extends JComponent implements Scrollable,
                                                     DropTargetComponent
{
  // For synchronization
  TreeTable           fTreeTable;

  CellRendererPane    fCellRendererPane = new CellRendererPane();
  ColumnModel         fColumnModel;
  ColumnHeader        fColumnHeader;
  TreeTableDataModel  fDataModel;
  int                 fHeight;
  TreeNode            fRoot;
  TreeModelListener   fDataModelListener = new TreeModelListener();

  // Selection/focus
  TreeNode            fCaret;
  TreeNode            fAnchor;
  SelectionManager    fSelection = new MultiSelectionManager();
  int                 fHitX, fHitY;
  Point               fHitPoint;
  boolean             fDragging = false;
  Point               fTooltipPoint = null;

  // Editing
  boolean             fEditing = false;
  CellEditor          fEditor;
  TreeNode            fEditedNode;
  Column              fEditedColumn;
  boolean             fMousinessAfoot = false;
  boolean             fJustSelected = false;
  DropTarget          fDropTarget = null;

  // Attributes
  Object              fTreeColumn;
  int                 fIndentLevel = 16;
  int                 fRowHeight = 0;
  int                 fRowMargin = 0;
  boolean             fFixed = true;
  static final int    kGap = 4;

  // Properties
  boolean             fDrawPipes = false;

  // Images
  Icon                fPlusIcon;
  Icon                fMinusIcon;

  Color               fWindowColor;
  Color               fHighlightColor;

  //
  // Constructors
  //

  /**
   * Constructs a TreeTable with the given data model.
   */

  public TreeTable(TreeTableDataModel aDataModel) {
    fTreeTable = this;

    setLayout(null);
    add(fCellRendererPane);

    fColumnModel = new ColumnModelImp();
    fColumnModel.addColumnModelListener(new TreeColumnModelListener());
    fColumnHeader = new ColumnHeader(fColumnModel);

    TreeMouseListener mouseListener = new TreeMouseListener();
    addMouseListener(mouseListener);
    addMouseMotionListener(mouseListener);
    addFocusListener(new TreeFocusListener());

    fSelection = new MultiSelectionManager();
    fSelection.addSelectionListener(new TreeSelectionListener());

    updateUI();

    initializeKeys();

    if (aDataModel != null) {
      updateDataModel(aDataModel);
    }
  }

  //
  // Accessor functions
  //

  /**
   * Sets the current data model. A null value (should be)
   * acceptable.
   */

  public synchronized void setDataModel(TreeTableDataModel aDataModel) {
    if (fDataModel != null) {
      clearDataModel();
    }
    updateDataModel(aDataModel);

    resizeAndRepaint();
  }

  /**
   * Returns the current data model.
   *
   * @see TreeTableDataModel
   */

  public synchronized TreeTableDataModel getDataModel() {
     return fDataModel;
  }

  /**
   * Retuns the column model
   *
   * @see ColumnModel
   */

  public synchronized ColumnModel getColumnModel() {
    return fColumnModel;
  }

  /**
   * Adds a column to the tree table. The column is added at the end.
   */

  public void addColumn(Column aColumn) {
    fColumnModel.addColumn(aColumn);
  }

  /**
   * Adds a column at the given position. Existing columns may be
   * shifted to the right.
   */

  public void addColumnAt(Column aColumn, int aIndex) {
    fColumnModel.addColumnAt(aColumn, aIndex);
  }

  /**
   * Returns the ColumnHeader widget that allows the user to
   * view and manipulate column properties.
   */

  public ColumnHeader getColumnHeader() {
    return fColumnHeader;
  }

  /**
   * Returns the last object on which the user clicked. This
   * object is used for keyboard navigation.
   */

  public TreePath getCaret() {
    return buildTreePath(fCaret);
  }

  void setCaret(TreeNode aNode) {
    if (fCaret != null) {
      repaint(fCaret);
    }
    fCaret = aNode;
    if (fCaret != null) {
      repaint(fCaret);
    }
  }

  /**
   * Sets the last object on which the user clicked. This
   * object is used for keyboard navigation.
   */

  public void setCaret(TreePath aPath) {
    TreeNode node = findTreeNode(aPath);
    if (node != null) {
      setCaret(node);
    }
  }

  /**
   * Returns the interface for the tree table's selection manager
   */

  public SelectionManager getSelectionManager() {
    return fSelection;
  }

  //
  // Attributes
  //

  /**
   * Sets which column in which the tree indention and (eventually)
   * icons appear.
   */

  public void setTreeColumn(Object aID) {
    fTreeColumn = aID;
  }

  /**
   * Returns which column in which the tree indention and (eventually)
   * icons appear.
   */

  public Object getTreeColumn() {
    return fTreeColumn;
  }

  /**
   * Sets the number of pixels each level of the tree is indented
   * (16 or more is currently recommended.)
   */

  public void setIndentLevel(int aIndent) {
    fIndentLevel = aIndent;
  }

  /**
   * Returns the number of pixels each level of the tree is indented.
   */

  public int getIndentLevel() {
    return fIndentLevel;
  }

  /**
   * Sets a fixed row height. Overrides the per-row sizing feature.
   * A height of 0 enables the per-row sizing feature.
   */

  public void setRowHeight(int aHeight) {
    fRowHeight = aHeight;
  }

  /**
   * Returns the fixed row height
   * @see setRowHeight()
   */

  public int getRowHeight() {
    return fRowHeight;
  }

  /**
   * Sets whether all rows have the same height. Saves a tremendous
   * amount of time if true. Defaults to true.
   * @see isFixedHeight()
   */

  public void setFixedHeight(boolean aFixed) {
    fFixed = aFixed;
  }

  /**
   * Returns whether all rows have the same height. Saves a tremendous
   * amount of time if true.
   * @see setFixedHeight()
   */

  public boolean isFixedHeight() {
    return fFixed;
  }

  /**
   * Does nothing, yet.
   */

  public void setRowMargin(int aMargin) {
    fRowMargin = aMargin;
  }

  /**
   * Does nothing, yet.
   */

  public int getRowMargin() {
    return fRowMargin;
  }

  //
  // Selection functions
  //

  Enumeration getShiftRange(TreePath aPath) {
    if (fAnchor == null) {
      fAnchor = getVisibleRoot();
    }

    TreeNode node = findTreeNode(aPath);
    int anchorY = getNodeY(fAnchor);
    int nodeY = getNodeY(node);

    TreeNode first;
    TreeNode last;

    if (anchorY < nodeY) {
      first = fAnchor;
      last = node;
    } else {
      first = node;
      last = fAnchor;
    }

    Vector range = new Vector();
    boolean done = false;
    while (!done && first != null) {
      range.insertElementAt(buildTreePath(first), range.size());

      if (first == last) {
        done = true;
      } else {
        TreeNode next = null;

        if (!first.isCollapsed() && !first.isLeaf()) {
          first = first.getFirstChild();
        } else {
          next = first.getNextSibling();
          if (next == null) {
            TreeNode parent = first.getParent();
            while (next == null && parent != null) {
              next = parent.getNextSibling();
              parent = parent.getParent();
            }
          }
          first = next;
        }
      }
    }
    return range.elements();
  }

  void select(TreePath aPath, int aModifiers) {
    boolean shiftKey = (aModifiers & KeyEvent.SHIFT_MASK) != 0;
    boolean controlKey = (aModifiers & KeyEvent.CTRL_MASK) != 0;
    boolean rightMouse = (aModifiers & KeyEvent.BUTTON3_MASK) != 0;

    // Complex heuristic for selecting based on windows.
    // Probably wrong for other platforms.
    // Maybe we need a plugable UI after all.

    if (controlKey) {
      if (!rightMouse) {
        if (fSelection.isSelected(aPath)) {
          if (shiftKey) {
            fSelection.removeSelection(getShiftRange(aPath));
          } else {
            fSelection.removeSelection(aPath);
          }
        } else {
          if (shiftKey) {
            fSelection.addSelection(getShiftRange(aPath));
          } else {
            fSelection.addSelection(aPath);
          }
        }
      }
    } else {
      if (rightMouse) {
        if (!fSelection.isSelected(aPath)) {
          fSelection.setSelection(aPath);
        }
      } else {
        if (shiftKey) {
          fSelection.setSelection(getShiftRange(aPath));
        } else {
          fSelection.setSelection(aPath);
        }
      }
    }

    if (!shiftKey) {
      fAnchor = findTreeNode(aPath);
    }
  }

  /**
   * Selects the given TreePath.
   */

  public void select(TreePath aPath) {
    select(aPath, 0);
  }

  //
  // Navigation functions
  //

  /**
   * Navigation used for up arrow key. Does reverse in-order
   * traversal, skipping children of collapsed nodes.
   */

  void navigateUp(int aModifiers) {
    if (fCaret != null) {
      if (fCaret != getVisibleRoot()) {
        TreeNode prev, last;
        prev = fCaret.getPrevSibling();
        if (prev == null) {
          setCaret(fCaret.getParent());
        } else {
          setCaret((TreeNode) null);
          while (prev != null && fCaret == null) {
            if (!prev.isCollapsed() && prev.getFirstChild() != null) {
              prev = prev.getLastChild();
            } else {
              setCaret(prev);
            }
          }
        }
      }
    }

    if (fCaret == null) {
      setCaret(getVisibleRoot());
    }

    select(buildTreePath(fCaret), aModifiers);
    ensureVisible(fCaret);
  }

  /**
   * Navigation used for down arrow key. Does in-order
   * traversal, skipping children of collapsed nodes.
   */

  void navigateDown(int aModifiers) {
    if (fCaret != null) {
      TreeNode next = null;

      if (!fCaret.isCollapsed()) {
        next = fCaret.getFirstChild();
      }

      if (next == null) {
        next = fCaret.getNextSibling();
        if (next == null) {
          TreeNode parent = fCaret.getParent();
          while (next == null && parent != null) {
            next = parent.getNextSibling();
            parent = parent.getParent();
          }
        }
      }

      if (next != null) {
        setCaret(next);
      }
    } else {
      setCaret(getVisibleRoot());
    }

    select(buildTreePath(fCaret), aModifiers);
    ensureVisible(fCaret);
  }

  /**
   * Navigation used for left arrow key. First collapses node,
   * then moves to parent
   */

  void navigateLeft(int aModifiers) {
    if (fCaret != null) {
      if (fCaret.isLeaf() || fCaret.isCollapsed()) {
        fCaret = fCaret.getParent();
      } else {
        collapse(fCaret);
      }
    }

    if (fCaret == null) {
      setCaret(getVisibleRoot());
    }

    select(buildTreePath(fCaret), aModifiers);
    ensureVisible(fCaret);
  }

  /**
   * Navigation used for right arrow key. First expands node,
   * then moves to first child
   */

  void navigateRight(int aModifiers) {
    if (fCaret != null) {
      if (!fCaret.isLeaf()) {
        if (!fCaret.isCollapsed()) {
          fCaret = fCaret.getFirstChild();
        } else {
          expand(fCaret);
        }
      }
    }

    if (fCaret == null) {
      setCaret(getVisibleRoot());
    }

    select(buildTreePath(fCaret), aModifiers);
    ensureVisible(fCaret);
  }

  void navigateHome(int aModifiers) {
    setCaret(getVisibleRoot());
    select(buildTreePath(fCaret), aModifiers);
    ensureVisible(fCaret);
  }

  TreeNode findLastVisible() {
    TreeNode node = getVisibleRoot();
    TreeNode res = null;

    while (node != null) {
      res = node;
      if (node.getNextSibling() != null) {
        node = node.getNextSibling();
      } else {
        node = node.getFirstChild();
      }
    }
    return res;
  }

  void navigateEnd(int aModifiers) {
    setCaret(findLastVisible());
    if (fCaret == null) {
      setCaret(getVisibleRoot());
    }
    select(buildTreePath(fCaret), aModifiers);
    ensureVisible(fCaret);
  }


  /**
   * Moves the caret up one position, and selects that object.
   * (currently doesn't support scrolling)
   */

  public void navigateUp() {
    navigateUp(0);
  }

  /**
   * Moves the caret down one position, and selects that object.
   * (currently doesn't support scrolling)
   */

  public void navigateDown() {
    navigateDown(0);
  }

  /**
   * Returns whether or not the give path is visible. If the path
   * points to the root, and the root is not shown, the method
   * still returns true.
   */

  public boolean isVisible(TreePath aPath) {
    return (findTreeNode(aPath) != null);
  }

  /**
   * Attempts to tweek parent into displaying the given node.
   */

  void ensureVisible(TreeNode aNode) {
    ensureVisible(buildTreePath(aNode));
  }

  public void ensureVisible(TreePath aPath) {
    int y = getNodeY(aPath);
    int h = getNodeHeight(aPath);

    if (y != -1) {
      Container parent = getParent();
      if (parent != null && parent instanceof JViewport) {
        JViewport parentView = (JViewport) parent;
        Dimension viewSize = parentView.getExtentSize();
        Point viewPos = parentView.getViewPosition();

        if (y < viewPos.y) {
          viewPos.y = y;
          parentView.setViewPosition(viewPos);
        } else if ((y + h) > (viewPos.y + viewSize.height)) {
          viewPos.y = y - viewSize.height + h;
          parentView.setViewPosition(viewPos);
        }
      }
    }
  }

  int getNodeY(TreeNode aNode) {
    return getNodeY(buildTreePath(aNode));
  }

  /**
   * Returns the Y location of the give node
   */

  public int getNodeY(TreePath aPath) {
    Object path[] = aPath.getPath();
    int length = aPath.getLength();
    int index = 0;
    int y = 0;
    TreeNode traverse = fRoot;

    if (!fDataModel.showRoot()) {
      index = 1;
      if (traverse != null) {
        traverse = traverse.getFirstChild();
      }
    }

    while (index < length) {
      while (traverse != null && traverse.getData() != path[index]) {
        y += traverse.getTotalHeight();
        traverse = traverse.getNextSibling();
      }

      if (traverse == null) {
        return -1;
      }

      index++;
      if (index < length) {
        y += traverse.getNodeHeight();
        traverse = traverse.getFirstChild();
      }
    }

    return y;
  }

  public int getNodeHeight(TreePath aPath) {
    if(fRowHeight > 0) {
      return fRowHeight;
    }

    TreeNode node = findTreeNode(aPath);
    if (node == null) {
      return -1;
    }
    return node.getNodeHeight();
  }

  public int getColumnX(Column column) {
    int res = -1;
    if (column != null) {
      int idx = fColumnModel.getColumnIndex(column);
      int margin = fColumnModel.getColumnMargin();

      res = 0;
      for (int i = 0; i < idx; i++) {
        res += fColumnModel.getColumn(i).getWidth() + margin;
      }
    }

    return res;
  }

  //
  // Utility functions
  //

  TreeNode getVisibleRoot() {
    if (fDataModel == null) {
      return null;
    }

    if (fDataModel.showRoot()) {
      return fRoot;
    } else {
      return fRoot.getFirstChild();
    }
  }

  TreePath buildTreePath(TreeNode aNode) {
    if (aNode == null) {
      return null;
    }

    int depth = aNode.getDepth();
    Object path[] = new Object[depth + 1];

    while(aNode != null) {
      path[depth] = aNode.getData();
      aNode = aNode.getParent();
      depth--;
    }

    return new TreePath(path);
  }

  TreeNode findTreeNodeRecurse(TreeNode aNode, Object aPath[], int aFirst, int aLast) {
    while (aNode != null) {
      if (aNode.getData() == aPath[aFirst]) {
        if (aFirst == aLast) {
          return aNode;
        } else {
          return findTreeNodeRecurse(aNode.getFirstChild(), aPath, aFirst + 1, aLast);
        }
      } else {
        aNode = aNode.getNextSibling();
      }
    }
    return null;
  }

  /**
   * Finds a TreeNode given a path. If null is returned, either
   * the path is invalid, or the path isn't fully visible.
   */

  synchronized TreeNode findTreeNode(TreePath aPath) {
    return findTreeNodeRecurse(fRoot, aPath.getPath(), 0, aPath.getLength() - 1);
  }

  synchronized void resizeAndRepaint() {
    setSize(getPreferredSize());
    repaint();
  }

  synchronized void resizeAndRepaintFrom(TreePath aPath) {
    setSize(getPreferredSize());
    repaintFrom(aPath);
  }

  synchronized void clearDataModel() {
    fSelection.clearSelection();
    setCaret((TreeNode) null);

    fDataModel.removeTreeTableModelListener(fDataModelListener);
    fDataModel = null;

    TreeNode node = fRoot;
    while (node != null) {
      TreeNode next = node.getNextSibling();
      node.detachSelf();
      node = next;
    }
    fRoot = null;
  }

  synchronized void updateDataModel(TreeTableDataModel aDataModel) {
    fDataModel = aDataModel;

    if (fDataModel != null) {
      fDataModel.addTreeTableModelListener(fDataModelListener);
    }

    reload();
  }

  /**
   * Forces the tree to rebuild itself from scratch.
   * Doesn't repaint.
   */

  public void reload() {
    if (fDataModel != null) {
      Enumeration children = fDataModel.getChildren(null);
      if (children != null) {
        if (children.hasMoreElements()) {
          // First case where children are given as enumerations
          Object node = children.nextElement();
          fRoot = new TreeNode(fDataModel, null, node);
          TreeNode prev = fRoot;

          while (children.hasMoreElements()) {
            node = children.nextElement();
            prev.addSiblingAfter(new TreeNode(fDataModel, null, node));
            prev = prev.getNextSibling();
          }
        } else {
          fRoot = null;
        }
      } else {
        // Second case where children are give by getFirstChild,
        // getNextSibling
        Object node = fDataModel.getRoot();
        if (node != null) {
          fRoot = new TreeNode(fDataModel, null, node);
          TreeNode prev = fRoot;

          node = fDataModel.getNextSibling(node);
          while (node != null) {
            prev.addSiblingAfter(new TreeNode(fDataModel, null, node));
            prev = prev.getNextSibling();
            node = fDataModel.getNextSibling(node);
          }
        } else {
          fRoot = null;
        }
      }
    }
    initializeCached();
  }

  void initializeCached() {
    TreeNode node = getVisibleRoot();
    fHeight = 0;

    while (node != null) {
      fHeight += node.getTotalHeight();
      node = node.getNextSibling();
    }
  }

  void initializeKeys() {
    registerKeyboardAction(new NavigateUpAction(0),
                           KeyStroke.getKeyStroke(KeyEvent.VK_UP, 0),
                           WHEN_FOCUSED);
    registerKeyboardAction(new NavigateDownAction(0),
                           KeyStroke.getKeyStroke(KeyEvent.VK_DOWN, 0),
                           WHEN_FOCUSED);
    registerKeyboardAction(new NavigateLeftAction(0),
                           KeyStroke.getKeyStroke(KeyEvent.VK_LEFT, 0),
                           WHEN_FOCUSED);
    registerKeyboardAction(new NavigateRightAction(0),
                           KeyStroke.getKeyStroke(KeyEvent.VK_RIGHT, 0),
                           WHEN_FOCUSED);
    registerKeyboardAction(new NavigateHomeAction(0),
                           KeyStroke.getKeyStroke(KeyEvent.VK_HOME, 0),
                           WHEN_FOCUSED);
    registerKeyboardAction(new NavigateEndAction(0),
                           KeyStroke.getKeyStroke(KeyEvent.VK_END, 0),
                           WHEN_FOCUSED);
    registerKeyboardAction(new OpenAction(),
                           KeyStroke.getKeyStroke(KeyEvent.VK_ENTER, 0),
                           WHEN_FOCUSED);
  }

  void expand(TreeNode aNode) {
    fDataModel.setCollapsed(buildTreePath(aNode), false);
  }

  void collapse(TreeNode aNode) {
    fDataModel.setCollapsed(buildTreePath(aNode), true);
  }

  int getIndent(Column aColumn, TreeNode aNode) {
    int indent = 0;
    if (aColumn.getID().equals(fTreeColumn)) {
      Icon icon = fDataModel.getIcon(aNode.getData());
      indent = aNode.getVisibleDepth() * fIndentLevel + fIndentLevel +
        (icon != null ? icon.getIconWidth() : 0) + kGap;
    }
    return indent;
  }

  int drawPipes(Graphics g, TreeNode aNode, int x, int y, int w, int h) {
    TreeNode node = aNode;
    int depth = aNode.getVisibleDepth();
    x += depth * fIndentLevel;

    int midY = y + h / 2;

    g.setColor(Color.black);

    int iconWidth = 0;
    Icon icon = fDataModel.getIcon(aNode.getData());

    if (icon != null) {
      iconWidth = icon.getIconWidth();
      int iconY = midY - icon.getIconHeight() / 2;

      icon.paintIcon(this, g, x + fIndentLevel, iconY);

      Icon overlayIcon = fDataModel.getOverlayIcon(aNode.getData());
      if (overlayIcon != null) {
        overlayIcon.paintIcon(this, g, x + fIndentLevel, iconY);
      }
    }

    while (node != null && node.getVisibleDepth() >= 0) {
      int midX = x + fIndentLevel / 2;

      if (fDrawPipes) {
        if (node.getNextSibling() != null) {
          g.drawLine(midX, y, midX, y + h);
        }

        if (aNode == node) {
          g.drawLine(midX, midY, x + fIndentLevel, midY);
          if (node.getParent() != null) {
            g.drawLine(midX, midY, midX, y);
          }
        }
      }

      if (node == aNode && !node.isLeaf()) {
        if (node.isCollapsed()) {
          fPlusIcon.paintIcon(this, g, x, y);
        } else {
          fMinusIcon.paintIcon(this, g, x, y);
        }
      }

      node = node.getParent();
      x -= fIndentLevel;
    }

    return fIndentLevel + depth * fIndentLevel + iconWidth + kGap;
  }

  /**
   * Recursive part of <code>paint()</code>.
   * @see #paint
   */

  static int fPaintedRows;

  void paintRecurse(Graphics g, TreeNode aRoot, int y) {
    int x;
    int w, h;
    boolean selected = false;
    boolean done = false;
    TreeNode node = aRoot;

    int columnMargin = fColumnModel.getColumnMargin();

    Rectangle rect = g.getClipBounds();

    while (node != null && !done) {
      TreePath path = buildTreePath(node);

      h = node.getNodeHeight();
      x = 0;
      selected = fSelection.isSelected(path);

      if (y + h >= rect.y) { // Paint only visible stuff
        if (y < rect.y + rect.height) { // ditto

          fPaintedRows++;

          Enumeration columns = fColumnModel.getColumns();

          g.setColor(selected ? fHighlightColor : fWindowColor);
          g.fillRect(0, y, getWidth(), h);

          while (columns.hasMoreElements()) {
            Column column = (Column) columns.nextElement();
            CellRenderer renderer = column.getCellRenderer();
            w = column.getWidth();

            if (x + w >= rect.x && x < rect.x + rect.width) {
              renderer.setValue(node.getData(),
                                fDataModel.getData(node.getData(), column.getID()),
                                selected);

              Component component = renderer.getComponent();

              int dx = x;
              int dw = w;
              if (column.getID().equals(fTreeColumn)) {
                int pipeWidth = drawPipes(g, node, x, y, w, h);
                dx = x + pipeWidth;
                dw = w - pipeWidth;
              }

              fCellRendererPane.paintComponent(g, component, this, dx, y, dw, h);
            }

            x += w + columnMargin;
          }

          if (hasFocus() && node == fCaret) {
            g.setXORMode(Color.white);
            BasicGraphicsUtils.drawDashedRect(g, 0, y, getWidth(), h);
            g.setPaintMode();
          }

        } else {
          done = true;
        }
      }

      if (!done) {
        y += h;

        if (node.getFirstChild() != null && !node.isCollapsed()) {
          paintRecurse(g, node.getFirstChild(), y);
          y += node.getChildrenHeight();
        }
        node = node.getNextSibling();
      }
    }
  }

  /**
   * Recursive part of <code>hitTestNode()</code>
   * @see #hitTestNode
   */

  TreeNode hitTestNodeRecurse(TreeNode aFirst, int aTop, int aY) {
    TreeNode node = aFirst;
    int nodeHeight, totalHeight;

    while (node != null) {
      nodeHeight = node.getNodeHeight();
      totalHeight = node.getTotalHeight();
      if (aY < aTop + totalHeight) { // hit node or child
        if (aY < aTop + nodeHeight) { // hit node
          return node;
        } else { // hit child
          return hitTestNodeRecurse(node.getFirstChild(), aTop + nodeHeight, aY);
        }
      }
      node = node.getNextSibling();
      aTop += totalHeight;
    }
    return null;
  }

  /**
   * Determines what node is under the given coordinate
   */

  synchronized TreeNode hitTestNode(int aY) {
    return hitTestNodeRecurse(getVisibleRoot(), 0, aY);
  }

  /**
   * Determines what column is under the given coordinate
   */

  Column hitTestColumn(int aX) {
    int columnMargin = fColumnModel.getColumnMargin();
    int w, x = 0;
    fHitX = -1;

    Enumeration columns = fColumnModel.getColumns();
    while (columns.hasMoreElements()) {
      Column column = (Column) columns.nextElement();
      w = column.getWidth() + columnMargin;

      if (aX < x + w) {
        fHitX = aX - x;
        return column;
      }
      x += w;
    }

    return null;
  }

  //
  // Component Overloads
  //

  public synchronized void paintComponent(Graphics g) {
    fPaintedRows = 0;

    paintRecurse(g, getVisibleRoot(), 0);

//    System.out.println("Painted " + fPaintedRows + " Rows");

    if (fHeight < getHeight()) {
      Rectangle rect = new Rectangle(0, fHeight, getWidth(), getHeight() - fHeight).
                       intersection(g.getClipBounds());
      g.setColor(fWindowColor);
      g.fillRect(rect.x, rect.y, rect.width, rect.height);
    }
  }

  void repaint(TreeNode aNode) {
    int y = getNodeY(aNode);
    int h = aNode.getNodeHeight();
    repaint(0, y, getWidth(), h);
  }

  public void repaint(TreePath aPath) {
    int y = getNodeY(aPath);
    int h = getNodeHeight(aPath);
    repaint(0, y, getWidth(), h);
  }

  public void repaintFrom(TreePath aPath) {
    int y = getNodeY(aPath);
    repaint(0, y, getWidth(), getHeight() - y);
  }

  public Dimension getPreferredSize() {
    return new Dimension(fColumnModel.getTotalColumnWidth(), fHeight);
  }

  public Dimension getMaximumSize() {
    return getPreferredSize();
  }

  public Dimension getMinimumSize() {
    return getPreferredSize();
  }

  public void updateUI() {
    super.updateUI();

    fPlusIcon = new ImageIcon(getClass().getResource("images/plus.gif"));
    fMinusIcon = new ImageIcon(getClass().getResource("images/minus.gif"));

    fWindowColor = UIManager.getColor("window");
    fHighlightColor = UIManager.getColor("textHighlight");
  }
  public boolean isOpaque() {
    return true;
  }

  public boolean getAutoscrolls() {
    return true;
  }

  public Point getToolTipLocation(MouseEvent aEvent) {
    return fTooltipPoint;
  }

  public String getToolTipText(MouseEvent aEvent) {
    TreeNode node = hitTestNode(aEvent.getY());
    if (node != null) {
      Column column = hitTestColumn(aEvent.getX());

      if (column != null) {
        int indent = getIndent(column, node);
        CellRenderer renderer = column.getCellRenderer();
        Component component = renderer.getComponent();
        Object data = fDataModel.getData(node.getData(), column.getID());
        renderer.setValue(node.getData(), data, false);
        Dimension size = component.getSize();
        if (size.width + indent > column.getWidth()) {
          fTooltipPoint =
            new Point(getColumnX(column) + indent, getNodeY(node));
          return data.toString();
        }
      }
    }
    fTooltipPoint = null;
    return null;
  }

  //
  // Scrollable interface
  //

  public Dimension getPreferredScrollableViewportSize() {
    return getPreferredSize();
  }

  public int getScrollableBlockIncrement(Rectangle aVisible, int aOrient,
                                         int aDirection) {
    if (aOrient == SwingConstants.VERTICAL) {
      return aVisible.height;
    } else {
      return aVisible.width;
    }
  }

  public int getScrollableUnitIncrement(Rectangle aVisible, int aOrient,
                                        int aDirection) {
    if (aOrient == SwingConstants.VERTICAL) {
      if (fRowHeight != 0) {
        return fRowHeight + fRowMargin;
      }
    }
    return 10;
  }

  public boolean getScrollableTracksViewportHeight() {
    return false;
  }

  public boolean getScrollableTracksViewportWidth() {
    return false;
  }

  //
  // DropTargetComponentInterface
  //

  public void setDropTarget(DropTarget dt) throws IllegalArgumentException {
    fDropTarget = dt;
  }

  public DropTarget getDropTarget() {
    return fDropTarget;
  }

  //
  // TreeMouseListener class
  //

  class TreeMouseListener extends MouseAdapter implements MouseMotionListener {

    boolean isContextClick(MouseEvent aEvent) {
      return (aEvent.isPopupTrigger() ||
             (aEvent.getModifiers() & MouseEvent.BUTTON3_MASK) != 0);
    }

    MouseEvent convertMouseEvent(MouseEvent aEvent) {
      return SwingUtilities.convertMouseEvent(fTreeTable, aEvent,
                                       fEditor.getCellEditorComponent());
    }

    void dispatchMouseEvent(MouseEvent aEvent) {
     fEditor.getCellEditorComponent().dispatchEvent(convertMouseEvent(aEvent));
    }

    void checkEditor(MouseEvent aEvent, Column column, TreeNode node) {
      fEditor = column != null ? column.getCellEditor() : null;
      fEditing = false;

      if (fEditor != null) {
        TreePath path = buildTreePath(node);
        fEditor.setValue(node.getData(),
                         fDataModel.getData(node.getData(),column.getID()),
                         fSelection.isSelected(path));
        if (fEditor.isCellEditable(aEvent)) {
          fEditedColumn = column;
          fEditedNode = node;
          fEditor.addCellEditorListener(new TreeCellEditorListener());
          Component editorComponent = fEditor.getCellEditorComponent();
          Dimension preferredSize = editorComponent.getPreferredSize();
          add(editorComponent);
          int x = getColumnX(column);
          int w = column.getWidth();
          int indent = getIndent(column, node);
          x += indent;
          w -= indent;

          int y = getNodeY(path);
          int h = getNodeHeight(path);
          editorComponent.setLocation(x, y);
          editorComponent.setSize(w, h);

          fEditing = true;
          fEditor.startCellEditing(convertMouseEvent(aEvent));
        }
      }
    }

    public void mousePressed(MouseEvent aEvent) {
      fJustSelected = false;
      fMousinessAfoot = true;
      fHitPoint = new Point(aEvent.getPoint());
      fDragging = false;

      TreeNode node = hitTestNode(aEvent.getY());
      if (node != null) {
        Column column = hitTestColumn(aEvent.getX());

        if (column != null && !isContextClick(aEvent)) {
          if (column.getID().equals(fTreeColumn)) {
            int plusX = node.getVisibleDepth() * fIndentLevel;

            if (fHitX > plusX && fHitX < (plusX + fIndentLevel)) {
              if (node.isCollapsed()) {
                expand(node);
              } else {
                collapse(node);
              }
              requestFocus();
              fJustSelected = true; // Pretend we did a selection to
                                    // Avoid triggering editor
              return; // Don't do selection
            }
          }
        }

        if (fEditing) {
          if (fEditedColumn == column && fEditedNode == node) {
            dispatchMouseEvent(aEvent);
            return;
          } else {
            fEditor.cancelCellEditing();
            fEditing = false;
          }
        }

        checkEditor(aEvent, column, node);

        if (!fEditing) {
          requestFocus();
          setCaret(node);
          select(buildTreePath(fCaret), aEvent.getModifiers());
        }
      } else {
        requestFocus();
      }
    }

    public void mouseReleased(MouseEvent aEvent) {
      fDragging = false;
      if (fEditing) {
        dispatchMouseEvent(aEvent);
      } else if (isContextClick(aEvent)) {
        fSelection.contextClickSelection(aEvent);
      } else if (!fJustSelected) {
        TreeNode node = hitTestNode(aEvent.getY());
        if (node != null) {
          Column column = hitTestColumn(aEvent.getX());
          checkEditor(aEvent, column, node);
        }
      }
      fMousinessAfoot = false;
      fJustSelected = false;
      fHitPoint = null;
    }

    public void mouseClicked(MouseEvent aEvent) {
      if (fEditing) {
        dispatchMouseEvent(aEvent);
      } else if (aEvent.getClickCount() == 2) {
        fSelection.doubleClickSelection(aEvent);
      }
    }

    public void mouseMoved(MouseEvent aEvent) {
      if (fEditing) {
        dispatchMouseEvent(aEvent);
      }
    }

    public void mouseDragged(MouseEvent aEvent) {
      if (fEditing) {
        dispatchMouseEvent(aEvent);
      } else if (!fDragging && fHitPoint != null) {
        Rectangle box =
          DragSource.getDragSource(fTreeTable).getDragThresholdBBox(fTreeTable,
                                                                    fHitPoint);

        if (!box.contains(aEvent.getPoint())) {
          fSelection.dragSelection(aEvent);
        }
        fDragging = true;
      }
    }
  }

  //
  // TreeCellEditorListener class
  //

  class TreeCellEditorListener implements CellEditorListener {
    public void editingStarted(ChangeEvent aEvent) {
      fEditing = true;
    }

    public void editingStopped(ChangeEvent aEvent) {
      fEditing = false;
      fDataModel.setData(fEditedNode.getData(), fEditedColumn.getID(), fEditor.getCellEditorValue());
      Rectangle edRect = fEditor.getCellEditorComponent().getBounds();
      remove(fEditor.getCellEditorComponent());
      repaint(edRect);
      fEditor.removeCellEditorListener(this);
    }

    public void editingCanceled(ChangeEvent aEvent) {
      fEditing = false;
      Rectangle edRect = fEditor.getCellEditorComponent().getBounds();
      remove(fEditor.getCellEditorComponent());
      repaint(edRect);
      fEditor.removeCellEditorListener(this);
    }
  }

  //
  // TreeColumnModelListener Class
  //

  class TreeColumnModelListener implements ColumnModelListener {
    public void columnAdded(ColumnModelEvent e) {
      resizeAndRepaint();
    }

    public void columnRemoved(ColumnModelEvent e) {
      resizeAndRepaint();
    }

    public void columnMoved(ColumnModelEvent e) {
      repaint();
    }

    public void columnMarginChanged(ChangeEvent e) {
      resizeAndRepaint();
    }

    public void columnWidthChanged(ColumnModelEvent e) {
      resizeAndRepaint();
    }

    public void columnSelectionChanged(ChangeEvent e) {
    }
  }

  class TreeModelListener implements TreeTableModelListener {
    public void nodeExpanded(TreeTableModelEvent aEvent) {
      synchronized (fTreeTable) {
        // We take the leap of faith here that we're not
        // expanding an already expanded node
        TreeNode node = findTreeNode(aEvent.getPath());
        fHeight += node.expand();
      }
      resizeAndRepaintFrom(aEvent.getPath());
    }

    public void nodeCollapsed(TreeTableModelEvent aEvent) {
      synchronized (fTreeTable) {
        // We take the leap of faith here that we're not
        // collapsing an already collapsed node
        TreeNode node = findTreeNode(aEvent.getPath());

        if (fCaret != null && fCaret.isChildOf(node)) {
          if (fSelection.isSelected(buildTreePath(fCaret))) {
            select(buildTreePath(node), 0);
          }
          setCaret(node);
        }

        fHeight += node.collapse();
      }

      resizeAndRepaintFrom(aEvent.getPath());
    }

    public void nodeInserted(TreeTableModelEvent aEvent) {
      synchronized (fTreeTable) {
        TreePath path = aEvent.getPath();
        if (path.getPath().length == 0) { // something at the root changed
          reload();
          resizeAndRepaint();
        } else {
          TreeNode node = findTreeNode(path);
          if (node != null) { // visible node changed
            node.reload(); // Redo everything for now
            resizeAndRepaintFrom(aEvent.getPath());
          }
        }
      }
    }

    public void nodeDeleted(TreeTableModelEvent aEvent) {
      synchronized (fTreeTable) {
        TreePath path = aEvent.getPath();
        if (path.getPath().length == 0) { // something at the root changed
          reload();
          resizeAndRepaint();
        } else {
          TreeNode node = findTreeNode(path);
          if (node != null) { // visible node changed
            node.reload(); // Redo everything for now
            resizeAndRepaintFrom(aEvent.getPath());
          }
        }
      }
    }

    public void nodeChanged(TreeTableModelEvent aEvent) {
      repaint(aEvent.getPath());
    }
  }

  //
  // TreeSelectionListener class
  //

  class TreeSelectionListener implements SelectionListener {
    public void selectionChanged(SelectionEvent aEvent) {
//      repaint();
      int i;
      Object added[] = aEvent.getAdded();
      Object removed[] = aEvent.getRemoved();

      if (added != null) {
        if (fMousinessAfoot)
          fJustSelected = true;

        for (i = 0; i < added.length; i++) {
          repaint((TreePath) added[i]);
        }
      }
      if (removed != null) {
        for (i = 0; i < removed.length; i++) {
          repaint((TreePath) removed[i]);
        }
      }
    }

    public void selectionDoubleClicked(MouseEvent aEvent) {
    }
    public void selectionContextClicked(MouseEvent aEvent) {
    }
    public void selectionDragged(MouseEvent aEvent) {
    }
  }

  //
  // TreeFocusListener class
  //

  class TreeFocusListener implements FocusListener {
    public void focusGained(FocusEvent aEvent) {
      repaint();
    }
    public void focusLost(FocusEvent aEvent) {
      repaint();
    }
  }

  //
  // ActionListener Classes
  //

  class NavigateUpAction implements ActionListener {
    int fModifiers;

    NavigateUpAction(int aModifiers) {
      fModifiers = aModifiers;
    }

    public void actionPerformed(ActionEvent aEvent) {
      navigateUp(fModifiers);
    }
  }

  class NavigateDownAction implements ActionListener {
    int fModifiers;

    NavigateDownAction(int aModifiers) {
      fModifiers = aModifiers;
    }

    public void actionPerformed(ActionEvent aEvent) {
      navigateDown(fModifiers);
    }
  }

  class NavigateLeftAction implements ActionListener {
    int fModifiers;

    NavigateLeftAction(int aModifiers) {
      fModifiers = aModifiers;
    }

    public void actionPerformed(ActionEvent aEvent) {
      navigateLeft(fModifiers);
    }
  }

  class NavigateRightAction implements ActionListener {
    int fModifiers;

    NavigateRightAction(int aModifiers) {
      fModifiers = aModifiers;
    }

    public void actionPerformed(ActionEvent aEvent) {
      navigateRight(fModifiers);
    }
  }

  class NavigateHomeAction implements ActionListener {
    int fModifiers;

    NavigateHomeAction(int aModifiers) {
      fModifiers = aModifiers;
    }

    public void actionPerformed(ActionEvent aEvent) {
      navigateHome(fModifiers);
    }
  }

  class NavigateEndAction implements ActionListener {
    int fModifiers;

    NavigateEndAction(int aModifiers) {
      fModifiers = aModifiers;
    }

    public void actionPerformed(ActionEvent aEvent) {
      navigateEnd(fModifiers);
    }
  }

  class OpenAction implements ActionListener {
    public void actionPerformed(ActionEvent aEvent) {
      fSelection.doubleClickSelection(null);
    }
  }

  //
  // TreeNode Class
  //

  class TreeNode {
    Object    fData;

    TreeNode  fParent;
    TreeNode  fFirstChild;
    TreeNode  fNextSibling;

    boolean   fBuilt = false;
    boolean   fFreeOnCollapse = true;
    boolean   fInvalid = true;
    boolean   fIsLeaf = false;
    int       fChildrenHeight = 0;
    int       fDepth = -1;
    int       fNodeHeight = 0;

    //
    // Constructor
    //

    public TreeNode(TreeTableDataModel aDataModel, TreeNode aParent, Object aData) {
      fDataModel = aDataModel;
      fData = aData;
      fParent = aParent;
      fIsLeaf = aDataModel.isLeaf(fData);

      if (!fIsLeaf && !isCollapsed()) {
        expand();
      }
    }

    //
    // Building
    //

    final void addChild(TreeNode aNode) {
      addChildAfter(getLastChild(), aNode);
    }

    final void addChildAfter(TreeNode aPrev, TreeNode aNode) {
      aNode.fParent = this;

      if (aPrev == null) { // add at beginning
        aNode.fNextSibling = fFirstChild;
        fFirstChild = aNode;
      } else {
        aPrev.addSiblingAfter(aNode);
      }
    }

    final void addSiblingAfter(TreeNode aNode) {
      aNode.fNextSibling = fNextSibling;
      fNextSibling = aNode;
    }

    //
    // Unbuilding
    //

    final int remove() {
      if (fParent != null) {
        fParent.removeChild(this);
      } else {
        TreeNode prev = getPrevSibling();
        prev.fNextSibling = fNextSibling;
      }
      return -getHeight();
    }

    final int removeChild(TreeNode aNode) {
      if (fFirstChild == aNode && aNode != null) {
        fFirstChild = aNode.fNextSibling;
      }
      aNode.fParent = null;
      return aNode.remove();
    }

    /**
     * Make garbage collection possible
     */

    final void detachChildren() {
      TreeNode node = fFirstChild;
      while (node != null) {
        TreeNode next = node.fNextSibling;
        node.detachSelf();
        node = next;
      }
      fFirstChild = null;
    }

    final void detachSelf() {
      detachChildren();
      fParent = null;
      fNextSibling = null;
    }

    void invalidateBranch() {
      TreeNode node = this;
      while (node != null) {
        fInvalid = true;
        node = node.getParent();
      }
    }

    public final int collapse() {
      int res = -getChildrenHeight();

      if (fFreeOnCollapse) {
        detachChildren();
        fBuilt = false;
      }
      invalidateBranch();

      return res;
    }

    public final int expand() {
      if (!fBuilt) {
        build();
      }

      invalidateBranch();

      return getChildrenHeight();
    }

    public final int reload() {
      fIsLeaf = fDataModel.isLeaf(fData);

      if (fIsLeaf || isCollapsed()) {
        return 0;
      }
      int oldChildHeight = getChildrenHeight();

      detachChildren();
      build();

      invalidateBranch();

      return getChildrenHeight() - oldChildHeight;
    }

    final void build() {
      if (!fIsLeaf) {
        Enumeration children = fDataModel.getChildren(fData);
        if (children == null) {
          Object node = fDataModel.getChild(fData);
          while (node != null) {
            addChild(new TreeNode(fDataModel, this, node));
            node = fDataModel.getNextSibling(node);
          }
        } else {
          while (children.hasMoreElements()) {
            addChild(new TreeNode(fDataModel, this, children.nextElement()));
          }
        }
      }
    }

    public final boolean isCollapsed() {
      return fDataModel.isCollapsed(buildTreePath(this));
    }

    public final boolean isLeaf() {
      return fIsLeaf;
    }

    public final int getVisibleDepth() {
      return fDataModel.showRoot() ? getDepth() : getDepth() - 1;
    }

    public final int getDepth() {
      if (fDepth < 0) {
        if (getParent() != null) {
          fDepth = getParent().getDepth() + 1;
        } else {
          fDepth = 0;
        }
      }
      return fDepth;
    }

    public final TreeNode getParent() {
      return fParent;
    }

    public final TreeNode getPrevSibling() {
      TreeNode node = (fParent != null) ? fParent.getFirstChild() : fRoot;
      while (node != null) {
        if (node.fNextSibling == this) {
          return node;
        }
        node = node.fNextSibling;
      }
      return null;
    }

    public final TreeNode getNextSibling() {
      return fNextSibling;
    }

    public final TreeNode getFirstChild() {
      return fFirstChild;
    }

    public final TreeNode getLastChild() {
      TreeNode node = fFirstChild;
      while (node != null && node.fNextSibling != null) {
        node = node.fNextSibling;
      }
      return node;
    }

    public final boolean isChildOf(TreeNode aNode) {
      TreeNode parent = fParent;
      while (parent != null) {
        if (parent == aNode) {
          return true;
        }
        parent = parent.fParent;
      }
      return false;
    }

    public final Object getData() {
      return fData;
    }

    public final int getNodeHeight() {
      if (fRowHeight != 0) {
        return fRowHeight;
      }
      if (fNodeHeight == 0) {
        Enumeration columns = fColumnModel.getColumns();
        while (columns.hasMoreElements()) {
          Column column = (Column) columns.nextElement();
          CellRenderer renderer = column.getCellRenderer();

          renderer.setValue(fData,
                            fDataModel.getData(fData, column.getID()),
                            false);

          Component component = renderer.getComponent();
          Dimension size = component.getPreferredSize();
          if (size.height > fNodeHeight) {
            fNodeHeight = size.height;
          }
        }
        Icon icon = fDataModel.getIcon(fData);
        if (icon != null && icon.getIconHeight() > fNodeHeight) {
          fNodeHeight = icon.getIconHeight();
        }
      }

      // If we're fixed, we can avoid this calculation
      if (fFixed) {
        fRowHeight = fNodeHeight;
      }

      return fNodeHeight;
    }

    public final int getChildrenHeight() {
      if (fInvalid) {
        fChildrenHeight = 0;
        TreeNode node = getFirstChild();
        while (node != null) {
          fChildrenHeight += node.getTotalHeight();
          node = node.getNextSibling();
        }
      }
      return fChildrenHeight;
    }

    public final int getTotalHeight() {
      return  getNodeHeight() + (isCollapsed() ? 0 : getChildrenHeight());
    }
  }

}
