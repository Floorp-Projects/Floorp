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

import java.awt.Color;
import java.awt.Component;
import java.awt.Cursor;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Graphics;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.awt.event.MouseMotionListener;

import java.util.Enumeration;

import javax.swing.CellRendererPane;
import javax.swing.JComponent;
import javax.swing.UIManager;
import javax.swing.event.ChangeEvent;

public class ColumnHeader extends JComponent implements ColumnModelListener {
  ColumnModel fColumnModel;
  CellRendererPane fCellRendererPane = new CellRendererPane();

  static final int RESIZE_MARGIN = 4;
  static final int DRAG_THRESHOLD = 4;

  boolean fDynamicUpdate = true;

  // Set by hit test
  boolean fOverResize;
  int     fHitOffset;
  Column  fPressedColumn;

  HeaderMouseListener fMouseListener;

  //
  // HeaderMouseListener
  //

  class HeaderMouseListener implements MouseMotionListener, MouseListener {
    boolean fResizing;
    int     fOverColumn;
    int     fDragOverColumn;
    int     fHitX;
    int     fHitColumn;
    int     fOldSize;
    int     fOldDragX = -1;
    int     fDragOffset;
    boolean fDragging;

    // MouseMotionListener Interface

    public void mouseMoved(MouseEvent aEvent) {
      fOverColumn = hitTest(aEvent.getX());
      if (fOverResize) {
        setCursor(Cursor.getPredefinedCursor(Cursor.E_RESIZE_CURSOR));
      } else {
        setCursor(Cursor.getPredefinedCursor(Cursor.DEFAULT_CURSOR));
      }
    }

    public void mouseDragged(MouseEvent aEvent) {
      if (fHitColumn < 0) {
        return;
      }
      int x = aEvent.getX();
      if (fResizing) {
        if (fDynamicUpdate) {
          updateColumnSize(x);
        } else {
          paintResizeLine(x);
        }
      } else if (Math.abs(x - fHitX) > DRAG_THRESHOLD) {
        fDragging = true;
        fDragOverColumn = hitTest(x);
        if (fDynamicUpdate) {
          int threshhold = 0;
          if (fDragOverColumn != -1) {
            if (fDragOverColumn < fHitColumn) {
              threshhold = getColumnX(fDragOverColumn) +
                           fColumnModel.getColumn(fHitColumn).getWidth();
            } else {
              threshhold = getColumnX(fDragOverColumn) +
                           fColumnModel.getColumn(fDragOverColumn).getWidth() -
                           fColumnModel.getColumn(fHitColumn).getWidth();
            }
          }

          if (fDragOverColumn != -1 &&
              ((fDragOverColumn < fHitColumn &&
                aEvent.getX() < threshhold) ||
                (fDragOverColumn > fHitColumn &&
                aEvent.getX() > threshhold))) {
            paintDraggedHeader(-1);
            fColumnModel.moveColumn(fHitColumn, fDragOverColumn);
            fHitColumn = fDragOverColumn;
          } else {
            paintDraggedHeader(aEvent.getX() + fDragOffset);
          }
        } else {
          paintDraggedHeader(aEvent.getX() + fDragOffset);
        }
      }
    }

    // MouseListener Interface

    public void mouseEntered(MouseEvent aEvent) {

    }

    public void mouseExited(MouseEvent aEvent) {

    }

    public void mouseClicked(MouseEvent aEvent) {
    }

    public void mousePressed(MouseEvent aEvent) {
      fHitX = aEvent.getX();
      fHitColumn = fOverColumn;
      if (fHitColumn > -1) {
        Column column = fColumnModel.getColumn(fHitColumn);
        if (fOverResize) {
          fResizing = true;
          fOldSize = column.getWidth();
        } else {
          fDragOffset = fHitOffset;
          if (column.isSelectable()) {
            fPressedColumn = column;
            repaint();
          }
        }
      }
    }

    public void mouseReleased(MouseEvent aEvent) {
      if (fResizing && !fDynamicUpdate) {
        paintResizeLine(-1);
        updateColumnSize(aEvent.getX());
      } else if (fDragging) {
        fDragging = false;
        paintDraggedHeader(-1);
        if (!fDynamicUpdate) {
          if (fDragOverColumn != fHitColumn && fDragOverColumn != -1) {
            fColumnModel.moveColumn(fHitColumn, fDragOverColumn);
          }
        }
      } else {
        if (fPressedColumn != null) {
          fColumnModel.setSelectedColumn(fPressedColumn);
        }
      }

      fHitX = -1;
      fHitColumn = -1;
      fResizing = false;
      fPressedColumn = null;

      repaint();
    }

    void updateColumnSize(int aMouseX) {
      Column column = fColumnModel.getColumn(fHitColumn);
      int newSize = fOldSize + (aMouseX - fHitX);

      if (newSize >= column.getMinWidth() &&
          newSize <= column.getMaxWidth()) {
        column.setWidth(newSize);
      }
    }

    void paintResizeLine(int aMouseX) {
      Graphics g = getGraphics();
      g.setXORMode(Color.white);
      g.setColor(Color.black);

      if (fOldDragX > -1) {
        g.drawLine(fOldDragX, 0, fOldDragX, getHeight());
      }
      if (aMouseX > -1) {
        g.drawLine(aMouseX, 0, aMouseX, getHeight());
      }
      g.setPaintMode();
      fOldDragX = aMouseX;
    }

    void paintDraggedHeader(int aMouseX) {
      Graphics g = getGraphics();
      g.setXORMode(Color.white);

      Column column = fColumnModel.getColumn(fHitColumn);

      int w, h;

      w = column.getWidth();
      h = getHeight();

      HeaderRenderer renderer = column.getHeaderRenderer();
      renderer.setValue(column, HeaderRenderer.DRAGGING);
      Component component = renderer.getComponent();

      if (fOldDragX > -1) {
        paintComponent(g, component, fOldDragX, 0, w, h);
      }
      if (aMouseX > -1) {
        paintComponent(g, component, aMouseX, 0, w, h);
      }

      g.setPaintMode();
      fOldDragX = aMouseX;
    }

    int getColumnX(int aIdx) {
      int res = 0;
      for (int i = 0; i < aIdx; i++) {
        res += fColumnModel.getColumn(i).getWidth();
      }
      return res;
    }
  }

  //
  // Constructor
  //

  /**
   * Takes a ColumnModel. The ColumnModel is generally shared with whatever
   * is the column header for.
   */

  public ColumnHeader(ColumnModel aColumnModel) {
    add(fCellRendererPane);

    fColumnModel = aColumnModel;
    fColumnModel.addColumnModelListener(this);

    updateUI();

    fMouseListener = new HeaderMouseListener();
    addMouseListener(fMouseListener);
    addMouseMotionListener(fMouseListener);
  }

  /**
   * Sets whether the UI repaints all columns as a column is
   * being resized or dragged. Set to false for better performance.
   */

  public void setDynamicUpdate(boolean aDynamic) {
    fDynamicUpdate = aDynamic;
  }

  /**
   * @return Whether the UI is repainting the columns as a column
   *         is being sized or dragged.
   */

  public boolean getDynamicUpdate() {
    return fDynamicUpdate;
  }

  //
  // Utility methods
  //

  int hitTest(int aX) {
    if (aX < 0) {
      return -1;
    }

    int index = 0;
    int w = 0;

    fOverResize = false;

    Enumeration columns = fColumnModel.getColumns();
    while (columns.hasMoreElements()) {
      Column column = (Column) columns.nextElement();
      w += column.getWidth();
      if (aX < w) {
        if (aX > w - RESIZE_MARGIN) {
          fOverResize = true;
        }
        fHitOffset = w - column.getWidth() - aX;
        return index;
      }
      index++;
    }

    return -1;
  }

  void paintComponent(Graphics g, Component component,
                      int x, int y, int w, int h) {
    fCellRendererPane.paintComponent(g, component, this, x, y, w, h);
  }

  //
  // ColumnModelListener Interface
  //

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
    repaint();
  }

  void resizeAndRepaint() {
    setSize(getPreferredSize());
    repaint();
  }

  //
  // Component Overloads
  //

  public void paint(Graphics g) {
    int margin = fColumnModel.getColumnMargin();
    int x = 0, y = 0;
    int w = 0, h = getHeight();

    Color bg = getBackground();
    for (Enumeration columnEnum = fColumnModel.getColumns();
         columnEnum.hasMoreElements(); ) {
      Column column = (Column) columnEnum.nextElement();

      // Easy Part
      w = column.getWidth() + margin;

      if (bg != null) {
        g.setColor(bg);
        g.fillRect(x, y, w, h);
      }

      // Hard Part
      HeaderRenderer renderer = column.getHeaderRenderer();
      if (column == fPressedColumn) {
        renderer.setValue(column, HeaderRenderer.DEPRESSED);
      } else if (column == fColumnModel.getSelectedColumn()) {
        renderer.setValue(column, HeaderRenderer.SELECTED);
      } else {
        renderer.setValue(column, HeaderRenderer.NORMAL);
      }
      Component component = renderer.getComponent();
      fCellRendererPane.paintComponent(g, component, this, x, y, w, h);

      x += w;
    }
    if (x < getWidth()) {
      if (bg != null) {
        g.setColor(bg);
        g.fillRect(x, y, getWidth() - x, h);
      }
    }
  }

  public Dimension getPreferredSize() {
    Dimension res = new Dimension();
    int margin = fColumnModel.getColumnMargin();
    for (Enumeration columnEnum = fColumnModel.getColumns();
         columnEnum.hasMoreElements(); ) {
      Column column = (Column) columnEnum.nextElement();

      // Easy Part
      res.width += column.getWidth() + margin;

      // Hard Part
      HeaderRenderer renderer = column.getHeaderRenderer();
      renderer.setValue(column, HeaderRenderer.NORMAL);
      Component component = renderer.getComponent();
      Dimension dim = component.getPreferredSize();
      if (dim.height > res.height) {
        res.height = dim.height;
      }
    }
    return res;
  }

  public Dimension getMinimumSize() {
    return getPreferredSize();
  }

  public Dimension getMaximumSize() {
    return getPreferredSize();
  }

  public void updateUI() {
    super.updateUI();

    setBackground(UIManager.getColor("control"));
  }
}
