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
 */

package grendel.composition;

import java.beans.*;

/** BeanInfo for AddressList:
*/
public class AddressListBeanInfo extends SimpleBeanInfo {
    private final static Class beanClass = AddressList.class;

    // getAdditionalBeanInfo method allows to return any number of additional
    // BeanInfo objects which provide information about the Bean that this BeanInfo
    // describes.
    public BeanInfo[] getAdditionalBeanInfo() {
        try {
            java.util.Vector v = new java.util.Vector();
            BeanInfo[] rv;
            BeanInfo b;
            Class c = beanClass.getSuperclass();

            while (c.isAssignableFrom(Object.class) != true) {
                b = Introspector.getBeanInfo(c);
                v.addElement(b);
                c = c.getSuperclass();
            }
                        rv = new BeanInfo[v.size()];
                        v.copyInto(rv);

                        return rv;
                }
                catch (IntrospectionException e) {
                    throw new Error(e.toString());
                }
        }

    // getIcon returns an image object which can be used by toolboxes, toolbars
    // to represent this bean. Icon images are in GIF format.
    public java.awt.Image getIcon(int iconKind)
    {
        if (iconKind == BeanInfo.ICON_COLOR_16x16 ||
            iconKind == BeanInfo.ICON_MONO_16x16)
        {
            java.awt.Image img = loadImage("images/AttachmentsListIcon16.gif");
            return img;
        }
        if (iconKind == BeanInfo.ICON_COLOR_32x32 ||
            iconKind == BeanInfo.ICON_MONO_32x32)
        {
            java.awt.Image img = loadImage("images/AttachmentsListIcon32.gif");
            return img;
        }
        return null;
    }
}
