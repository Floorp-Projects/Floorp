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

import java.awt.AWTEventMulticaster;
import java.util.Vector;
import java.util.Enumeration;
import java.util.StringTokenizer;

import javax.swing.event.ChangeEvent;

public class ColumnModelImp implements ColumnModel, ColumnChangeListener {
  Vector              fColumns = new Vector();
  ColumnModelListener fListeners = null;
  int                 fMargin = 0;
  Column              fSelectedColumn = null;

  public void addColumn(Column aColumn) {
    addColumnAt(aColumn, fColumns.size());
  }

  public void addColumnAt(Column aColumn, int aIndex) {
    aColumn.addColumnChangeListener(this);
    fColumns.insertElementAt(aColumn, aIndex);

    if (fListeners != null) {
      fListeners.columnAdded(new ColumnModelEvent(this, 0, aIndex));
    }
  }

  public void removeColumn(Column aColumn) {
    aColumn.removeColumnChangeListener(this);
    int idx = getColumnIndex(aColumn);
    if (idx > -1) {
      fColumns.removeElementAt(idx);
      if (fListeners != null) {
        fListeners.columnRemoved(new ColumnModelEvent(this, idx, 0));
      }
    }
  }

  public void moveColumn(int aSource, int aDest) {
    int i;
    if (aSource < aDest) {
      for (i = aSource; i < aDest; i++) {
        Object a = fColumns.elementAt(i);
        fColumns.removeElementAt(i);
        fColumns.insertElementAt(a, i + 1);
      }
      if (fListeners != null) {
        fListeners.columnMoved(new ColumnModelEvent(this, aSource, aDest));
      }
    } else if (aDest < aSource) {
      for (i = aSource; i > aDest; i--) {
        Object a = fColumns.elementAt(i);
        fColumns.removeElementAt(i);
        fColumns.insertElementAt(a, i - 1);
      }
      if (fListeners != null) {
        fListeners.columnMoved(new ColumnModelEvent(this, aSource, aDest));
      }
    } // else same, do nothing
  }

  public int getColumnCount() {
    return fColumns.size();
  }

  public Column getColumn(int aIndex) {
    return (Column) fColumns.elementAt(aIndex);
  }

  public Column getColumn(Object aID) {
    int count = getColumnCount();
    for (int i = 0; i < count; i++) {
      Column column = getColumn(i);
      if (column.getID().equals(aID)) {
        return column;
      }
    }
    return null;
  }

  private Column getColumn(String aID) {
    int count = getColumnCount();
    for (int i = 0; i < count; i++) {
      Column column = getColumn(i);
      if (column.getID().toString().equals(aID)) {
        return column;
      }
    }
    return null;
  }

  public Enumeration getColumns() {
    return fColumns.elements();
  }

  public int getColumnIndex(Column aColumn) {
    return fColumns.indexOf(aColumn);
  }

  public int getColumnIndexAtX(int aX) {
    if (aX < 0) {
      return -1;
    }

    int count = getColumnCount();
    int width = 0;
    for (int i = 0; i < count; i++) {
      Column column = getColumn(i);
      width += column.getWidth() + fMargin;
      if (aX <= width) {
        return i;
      }
    }
    return -1;
  }

  public void setColumnMargin(int aMargin) {
    fMargin = aMargin;
    if (fListeners != null) {
      fListeners.columnMarginChanged(new ChangeEvent(this));
    }
  }

  public int getColumnMargin() {
    return fMargin;
  }

  public int getTotalColumnWidth() {
    int i, res = 0;
    for (i = 0; i < fColumns.size(); i++) {
      res += getColumn(i).getWidth() + fMargin;
    }
    return res;
  }

  public void setSelectedColumn(Column aColumn) {
    fSelectedColumn = aColumn;
    if (fListeners != null) {
      fListeners.columnSelectionChanged(new ChangeEvent(this));
    }
  }

  public Column getSelectedColumn() {
    return fSelectedColumn;
  }

  public synchronized String getPrefString() {
    StringBuffer pref = new StringBuffer();
    Integer intVal = new Integer(0);
    for (int i = 0; i < fColumns.size(); i++) {
      pref.append(getColumn(i).getID().toString());
      pref.append(",");
      pref.append(Integer.toString(getColumn(i).getWidth()));
      if (i < fColumns.size() - 1) {
        pref.append(",");
      }
    }
    return pref.toString();
  }

  public synchronized void setPrefString(String aString) {
    StringTokenizer tokens = new StringTokenizer(aString, ",", false);
    int i = 0;
    while (tokens.hasMoreTokens()) {
      String id = tokens.nextToken();
      String width = tokens.nextToken();
      Column column = getColumn(id);

      if (column != null) {
        column.setWidth(Integer.parseInt(width));
        moveColumn(getColumnIndex(column), i);
      }

      i++;
    }
  }

  //
  // Listener support
  //

  public synchronized void addColumnModelListener(ColumnModelListener aListener) {
    fListeners = ColumnModelMulticaster.add(fListeners, aListener);
  }

  public synchronized void removeColumnModelListener(ColumnModelListener aListener) {
    fListeners = ColumnModelMulticaster.remove(fListeners, aListener);
  }

  //
  // ColumnListener Interface
  //

  public void columnWidthChanged(ChangeEvent aEvent) {
    int idx = fColumns.indexOf(aEvent.getSource());
    if (fListeners != null) {
      fListeners.columnWidthChanged(new ColumnModelEvent(this, 0, idx));
    }
  }
}

class ColumnModelMulticaster implements ColumnModelListener {
  ColumnModelListener a, b;

  public ColumnModelMulticaster(ColumnModelListener a, ColumnModelListener b) {
    this.a = a;
    this.b = b;
  }

  public static ColumnModelListener add(ColumnModelListener a, ColumnModelListener b) {
    if (a == null) return b;
    if (b == null) return a;

    return new ColumnModelMulticaster(a, b);
  }

  public static ColumnModelListener remove(ColumnModelListener a, ColumnModelListener b) {
    if (a == b || a == null) {
      return null;
    } else if (a instanceof ColumnModelMulticaster) {
      return ((ColumnModelMulticaster)a).remove(b);
    } else {
      return a;   // it's not here
    }
  }

  public ColumnModelListener remove(ColumnModelListener c) {
    if (c == a) return b;
    if (c == b) return a;

    ColumnModelListener a2 = remove(a, c);
    ColumnModelListener b2 = remove(b, c);
    if (a2 == a && b2 == b) {
        return this;  // it's not here
    }
    return add(a2, b2);
  }

  public void columnAdded(ColumnModelEvent aEvent) {
    a.columnAdded(aEvent);
    b.columnAdded(aEvent);
  }
  public void columnRemoved(ColumnModelEvent aEvent) {
    a.columnRemoved(aEvent);
    b.columnRemoved(aEvent);
  }
  public void columnMoved(ColumnModelEvent aEvent) {
    a.columnMoved(aEvent);
    b.columnMoved(aEvent);
  }
  public void columnMarginChanged(ChangeEvent aEvent) {
    a.columnMarginChanged(aEvent);
    b.columnMarginChanged(aEvent);
  }
  public void columnSelectionChanged(ChangeEvent aEvent) {
    a.columnSelectionChanged(aEvent);
    b.columnSelectionChanged(aEvent);
  }
  public void columnWidthChanged(ColumnModelEvent aEvent) {
    a.columnWidthChanged(aEvent);
    b.columnWidthChanged(aEvent);
  }
}
