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
import java.awt.Insets;
import java.awt.LayoutManager2;
import java.awt.IllegalComponentStateException;

class AttributeLayout implements LayoutManager2 {
  int fMargin = 4;
  Component fAttribute;
  Component fOperator;
  Component fValue;

  public static final String kAttribute = "attribute";
  public static final String kOperator = "operator";
  public static final String kValue = "value";

  public void addLayoutComponent(Component aComponent, Object aConstraint) {
    if (aConstraint.equals(kAttribute) && fAttribute == null) {
      fAttribute = aComponent;
    } else if (aConstraint.equals(kOperator) && fOperator == null) {
      fOperator = aComponent;
    } else if (aConstraint.equals(kValue) && fValue == null) {
      fValue = aComponent;
    } else {
      throw new IllegalComponentStateException();
    }
  }

  public void addLayoutComponent(String aConstraint, Component aComponent) {
    addLayoutComponent(aComponent, aConstraint);
  }

  public void removeLayoutComponent(Component aComponent) {
    if (aComponent == fAttribute) {
      fAttribute = null;
    } else if (aComponent == fOperator) {
      fOperator = null;
    } else if (aComponent == fValue) {
      fValue = null;
    }
  }

  public void layoutContainer(Container aParent) {
    int count = aParent.getComponentCount();
    Component c[] = aParent.getComponents();
    Insets insets = aParent.getInsets();
    Dimension size = aParent.getSize();
    int x = insets.top;
    int y = insets.left;
    int w = size.width - fMargin * (count - 1) - insets.left - insets.right;

    for (int i = 0; i < count; i++) {
      Dimension dim = c[i].getPreferredSize();
      if (c[i] == fValue) {
        dim.width = w / 2 + w % 4;
      } else {
        dim.width = w / 4;
      }
      c[i].setBounds(x, y, dim.width, dim.height);
      x += dim.width + fMargin;
    }
  }

  public Dimension preferredLayoutSize(Container aParent) {
    int count = aParent.getComponentCount();
    Component c[] = aParent.getComponents();
    Insets insets = aParent.getInsets();
    int h = 0;
    int w = 0;

    for (int i = 0; i < count; i++) {
      Dimension dim = c[i].getPreferredSize();
      if (dim.height > h) {
        h = dim.height;
      }
      w += dim.width;
    }
    w += insets.left + insets.right + (count - 1) * fMargin;
    h += insets.top + insets.bottom;

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
