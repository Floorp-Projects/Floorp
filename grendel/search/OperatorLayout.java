/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created: Will Scullin <scullin@netscape.com>,  4 Nov 1997.
 */

package grendel.search;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.IllegalComponentStateException;
import java.awt.Insets;
import java.awt.LayoutManager2;

class OperatorLayout implements LayoutManager2 {
  int fMargin = 4;
  Component fOperator;

  public final static String kOperator = "Operator";

  public void addLayoutComponent(Component aComponent, Object aConstraint) {
    addLayoutComponent((String) aConstraint, aComponent);
  }

  public void addLayoutComponent(String aConstraint, Component aComponent) {
    if (aConstraint != null && aConstraint.equals(kOperator)) {
      if (fOperator == null) {
        fOperator = aComponent;
      } else {
        throw new java.awt.IllegalComponentStateException();
      }
    }
  }

  public void removeLayoutComponent(Component aComponent) {
    if (aComponent == fOperator) {
      fOperator = null;
    }
  }

  public void layoutContainer(Container aParent) {
    int count = aParent.getComponentCount();
    Component c[] = aParent.getComponents();
    Insets insets = aParent.getInsets();
    Dimension size = aParent.getSize();
    int x = insets.left + fMargin;
    int y = insets.top + fMargin;
    int w = size.width - insets.left - insets.right - fMargin * 2;

    if (fOperator != null) {
      Dimension dim = fOperator.getPreferredSize();
      //x += dim.width + fMargin;
      w -= dim.width + fMargin;
    }

    for (int i = 0; i < count; i++) {
      if (c[i] != fOperator) {
        Dimension dim = c[i].getPreferredSize();
        c[i].setBounds(x, y, w, dim.height);
        y += dim.height + fMargin;
      }
    }

    if (fOperator != null) {
      Dimension dim = fOperator.getPreferredSize();
      //fOperator.setBounds(insets.left + fMargin,
      fOperator.setBounds(x + w + fMargin,
                          (y - dim.height) / 2,
                          dim.width, dim.height);
    }
  }

  public Dimension preferredLayoutSize(Container aParent) {
    int count = aParent.getComponentCount();
    Component c[] = aParent.getComponents();
    Insets insets = aParent.getInsets();
    int h = fMargin * 2;
    int w = 0;

    for (int i = 0; i < count; i++) {
      if (c[i] != fOperator) {
        Dimension dim = c[i].getPreferredSize();
        if (dim.width > w) {
          w = dim.width;
        }
        h += dim.height;
      }
    }

    w += insets.left + insets.right + fMargin * 2;
    h += insets.top + insets.bottom + (count - 1) * fMargin;

    if (fOperator != null) {
      Dimension dim = fOperator.getPreferredSize();
      w += dim.width;
    }

    return new Dimension(w, h);
  }

  public Dimension minimumLayoutSize(Container aParent) {
    return preferredLayoutSize(aParent);
  }

  public Dimension maximumLayoutSize(Container aParent) {
    return preferredLayoutSize(aParent);
  }

  public float getLayoutAlignmentX(Container aParent) {
    return 0;
  }

  public float getLayoutAlignmentY(Container aParent) {
    return 0;
  }

  public void invalidateLayout(Container aParent) {
  }
}
