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
 * Created: Will Scullin <scullin@netscape.com>, 26 Aug 1997.
 */

package grendel.widgets;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Cursor;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.LayoutManager2;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.awt.event.MouseMotionListener;

import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Vector;

import javax.swing.JComponent;
import javax.swing.UIManager;

/**
 * This class is used to create a divided pane with moveable dividers (NYI)
 * Components can be added with numeric weights that will determine how
 * much of the pane they will occupy:
 * <code>add(aComponent, new Float(.5))</code>
 * If no weight is given the default is 1.
 * <p>
 * (Soon to be) Moveable separators can be created by the
 * <code>createSeparator()</code>
 * method. The width of the separator is fixed in pixels.
 *
 * @author Will Scullin
 */

//
// Splitter Class
//

public class Splitter extends JComponent {
  Hashtable fWeights = new Hashtable();
  float     fTotalWeight = 0.0f;
  int       fTotalSeparators = 0;

  int       fOrientation;

  int       fMinSize = 16;

  SplitterSeparatorListener fListener;

  /**
   * Value to pass for a splitter that stacks components
   * vertically.
   */

  public static final int VERTICAL = 0;

  /**
   * Value to pass for a splitter that lays out components
   * horizontally.
   */

  public static final int HORIZONTAL = 1;

  //
  // Constructor
  //

  /**
   * Contructs a splitter container with the given orientation.
   */

  public Splitter(int aOrientation) {
    super();

    setLayout(new SplitterLayout());
    fOrientation = aOrientation;

    fListener = new SplitterSeparatorListener();

    updateUI();
  }

  public void updateUI() {
    super.updateUI();

    setBackground(UIManager.getColor("control"));
  }

  /**
   * Creates a separator of a given pixel size.
   */

  public Component createSeparator(int aSize) {
    return new Separator(fOrientation, aSize, false);
  }

  /**
   * Creates an separator of a given pixel size. If aFixed is
   * true, the separator is not user movable.
   */

  public Component createSeparator(int aSize, boolean aFixed) {
    return new Separator(fOrientation, aSize, aFixed);
  }

  /**
   * Returns the weight associated with a given component. This
   * number can be used to save and restore layout preferences.
   */

  public Float getWeight(Component aComponent) {
    return (Float) fWeights.get(aComponent);
  }

  /**
   * Sets the minimum size a component can be sized to.
   * Should be greater than zero for best results.
   */

  public void setMinSize(int aMinSize) {
    fMinSize = aMinSize;
  }

  /**
   * Returns the minimum size a component can be sized to.
   */

  public int getMinSize() {
    return fMinSize;
  }

  //
  // SplitterLayout
  //

  class SplitterLayout implements LayoutManager2 {

    public void addLayoutComponent(String aString, Component aComponent) {
      addLayoutComponent(aComponent, new Float(1.0));
    }

    public void addLayoutComponent(Component aComponent, Object aObject) {
      if (aComponent instanceof Separator) {
        Separator sep = (Separator) aComponent;
        sep.addSeparatorListener(fListener);
        fTotalSeparators += sep.getSeparatorSize();
      } else {
        Number weight = null;
        if (aObject instanceof Number) {
          weight = new Float(((Number) aObject).floatValue());
        } else {
          weight = new Float(1.0f);
        }
        fTotalWeight += weight.floatValue();
        fWeights.put(aComponent, weight);
      }
    }

    public void removeLayoutComponent(Component aComponent) {
      if (aComponent instanceof Separator) {
        Separator sep = (Separator) aComponent;
        sep.removeSeparatorListener(fListener);
        fTotalSeparators -= sep.getSeparatorSize();
      } else {
        Float weight = (Float) fWeights.get(aComponent);
        fWeights.remove(aComponent);
        fTotalWeight -= weight.floatValue();
      }
    }

    public void layoutContainer(Container aParent) {
      int i, count = aParent.getComponentCount();
      Component components[] = aParent.getComponents();

      Dimension parentSize = aParent.getSize();
      Insets insets = aParent.getInsets();
      parentSize.width -= insets.left + insets.right;
      parentSize.height -= insets.top + insets.bottom;

      Dimension availableSize = new Dimension(parentSize);

      if (fOrientation == Splitter.VERTICAL) {
        availableSize.height -= fTotalSeparators;
      } else {
        availableSize.width -= fTotalSeparators;
      }

      Point pos = new Point(insets.left, insets.right);
      Dimension size = new Dimension(availableSize);

      for (i = 0; i < count; i++) {

        if (i == count - 1) {
          if (fOrientation == Splitter.VERTICAL) {
            size.height = parentSize.height - pos.y;
          } else {
            size.width = parentSize.width - pos.x;
          }
        } else {
          if (components[i] instanceof Separator) {
            if (fOrientation == Splitter.VERTICAL) {
              size.height = ((Separator) components[i]).getSeparatorSize();
            } else {
              size.width = ((Separator) components[i]).getSeparatorSize();
            }
          } else {
            float weight = ((Number) fWeights.get(components[i])).floatValue();
            if (fOrientation == Splitter.VERTICAL) {
              size.height = (int)(parentSize.height * (weight / fTotalWeight));
            } else {
              size.width = (int)(parentSize.width * (weight / fTotalWeight));
            }
          }
        }

        components[i].setLocation(pos);
        components[i].setSize(size);

        if (fOrientation == Splitter.VERTICAL) {
          pos.y += size.height;
        } else {
          pos.x += size.width;
        }
      }
    }

    public float getLayoutAlignmentX(Container aParent) {
      return 0.5f;
    }

    public float getLayoutAlignmentY(Container aParent) {
      return 0.5f;
    }

    public void invalidateLayout(Container aParent) {
    }

    public Dimension preferredLayoutSize(Container aParent) {
      return aParent.getSize();
    }

    public Dimension minimumLayoutSize(Container aParent) {
      return preferredLayoutSize(aParent);
    }

    public Dimension maximumLayoutSize(Container aParent) {
      return preferredLayoutSize(aParent);
    }
  }

  class SplitterSeparatorListener implements SeparatorListener {
    int fOldPos = -1;

    public boolean separatorMoving(Separator aSep, int aDelta) {
      Dimension size = aSep.getParent().getSize();

      Graphics g = getGraphics();
      int pos;
      if (fOrientation == VERTICAL) {
        pos = aSep.getY() + aDelta;
        if (pos < fMinSize)
          pos = fMinSize;
        else if (pos > (size.height - aSep.getHeight() - fMinSize))
          pos = size.height - aSep.getHeight() - fMinSize;
      } else {
        pos = aSep.getX() + aDelta;
        if (pos < fMinSize)
          pos = fMinSize;
        else if (pos > (size.width - aSep.getWidth() - fMinSize))
          pos = size.width - aSep.getWidth() - fMinSize;
      }
      drawDividerLine(g, pos, aSep.getSeparatorSize());
      return true;
    }

    public void separatorMoved(Separator aSep, int aDelta) {
      Graphics g = getGraphics();
      drawDividerLine(g, -1, aSep.getSeparatorSize());

      Component components[] = getComponents();
      int count = getComponentCount();

      float beforeValue = 0, afterValue = 0;
      int beforeSize = 0, afterSize = 0;
      boolean before = true;

      for (int i = 0; i < count; i++) {
        if (components[i] instanceof Separator) {
          if (components[i] == aSep) {
            before = false;
          }
        } else {
          Dimension dim = components[i].getSize();
          int size = fOrientation == VERTICAL ?
                     dim.height :
                     dim.width;
          float value = ((Number) fWeights.get(components[i])).floatValue();
          if (before) {
            beforeValue += value;
            beforeSize += size;
          } else {
            afterValue += value;
            afterSize += size;
          }
        }
      }

      Dimension size = aSep.getParent().getSize();

      int pos;
      if (fOrientation == VERTICAL) {
        pos = aSep.getY() + aDelta;
        if (pos < fMinSize)
          pos = fMinSize;
        else if (pos > (size.height - aSep.getHeight() - fMinSize))
          pos = size.height - aSep.getHeight() - fMinSize;

        aDelta = pos - aSep.getY();
      } else {
        pos = aSep.getX() + aDelta;
        if (pos < fMinSize)
          pos = fMinSize;
        else if (pos > (size.width - aSep.getWidth() - fMinSize))
          pos = size.width - aSep.getWidth() - fMinSize;

        aDelta = pos - aSep.getX();
      }

      float beforeDelta = (float) (beforeSize + aDelta) / (float) beforeSize;
      float afterDelta = (float) (afterSize - aDelta) / (float) afterSize;

      before = true;
      fTotalWeight = 0;
      for (int i = 0; i < count; i++) {
        if (components[i] instanceof Separator) {
          if (components[i] == aSep) {
            before = false;
          }
        } else {
          float value = ((Number) fWeights.get(components[i])).floatValue();
          if (before) {
            value *= beforeDelta;
          } else {
            value *= afterDelta;
          }
          fWeights.put(components[i], new Float(value));
          fTotalWeight += value;
        }
      }

      invalidate();
      validate();
    }

    void drawDividerLine(Graphics g, int aPos, int aSize) {
      g.setXORMode(Color.white);
      g.setColor(Color.black);
      if (fOldPos != -1) {
        if (fOrientation == VERTICAL) {
          g.fillRect(0, fOldPos, getWidth(), aSize);
        } else {
          g.fillRect(fOldPos, 0, aSize, getHeight());
        }
      }
      if (aPos != -1) {
        if (fOrientation == VERTICAL) {
          g.fillRect(0, aPos, getWidth(), aSize);
        } else {
          g.fillRect(aPos, 0, aSize, getHeight());
        }
      }

      fOldPos = aPos;
      g.setPaintMode();
    }
  }
}

//
// SeparatorListener
//

interface SeparatorListener {
  public boolean separatorMoving(Separator aSep, int aDelta);
  public void separatorMoved(Separator aSep, int aDelta);
}

//
// Separator Class
//

class Separator extends JComponent {
  int fOrientation;
  int fSize;
  SplitterMouseListener fMouseListener;
  SeparatorListener     fListener;

  class SplitterMouseListener extends MouseAdapter implements MouseMotionListener {
    int hitX, hitY;
    boolean fDragging;
    int fDelta;

    public void mouseEntered(MouseEvent aEvent) {
      setCursor(Cursor.getPredefinedCursor(fOrientation == Splitter.VERTICAL ?
                                           Cursor.N_RESIZE_CURSOR :
                                           Cursor.E_RESIZE_CURSOR));
    }

    public void mousePressed(MouseEvent aEvent) {
      // XXX AWT bug work around
      // mousePressed gets called again if the cursor leaves the
      // frame. grr.
      if (!fDragging) {
        hitX = aEvent.getX();
        hitY = aEvent.getY();
        fDelta = 0;

        fDragging = true;
      }
    }

    public void mouseReleased(MouseEvent aEvent) {
      notifyMoved(fDelta);

      fDragging = false;
    }

    public void mouseMoved(MouseEvent aEvent) {
    }

    public void mouseDragged(MouseEvent aEvent) {
      int delta = fOrientation == Splitter.VERTICAL ?
                  aEvent.getY() - hitY :
                  aEvent.getX() - hitX;

      if (notifyMoving(delta)) {
        fDelta = delta;
      }
    }
  }

  public Separator(int aOrientation, int aSize, boolean aFixed) {
    fOrientation = aOrientation;
    fSize = aSize;

    if (!aFixed) {
      fMouseListener = new SplitterMouseListener();
      addMouseListener(fMouseListener);
      addMouseMotionListener(fMouseListener);
    }
  }

  public int getSeparatorSize() {
    return fSize;
  }

  public void paint(Graphics g) {
    Color bg = getBackground();
    if (bg != null) {
      Rectangle rect = getVisibleRect();

      g.setColor(bg);
      g.fillRect(rect.x, rect.y, rect.width, rect.height);
    }
  }

  public void addSeparatorListener(SeparatorListener aListener) {
    fListener = aListener;
  }

  public void removeSeparatorListener(SeparatorListener aListener) {
    if (aListener == fListener) {
      aListener = null;
    }
  }

  boolean notifyMoving(int aDelta) {
    if (fListener != null) {
      return fListener.separatorMoving(this, aDelta);
    }
    return true;
  }

  void notifyMoved(int aDelta) {
    if (fListener != null) {
      fListener.separatorMoved(this, aDelta);
    }
  }
}
