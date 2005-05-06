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
import java.lang.reflect.Method;

/** BeanInfo for AttachmentsList:
*/
public class AttachmentsListBeanInfo extends SimpleBeanInfo {
    private final static Class beanClass = AttachmentsList.class;

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

    // getPropertyDescriptors returns an array of PropertyDescriptors which describe
    // the editable properties of this bean.
    public PropertyDescriptor[] getPropertyDescriptors() {
/*
        try {
            PropertyDescriptor prop1 = new PropertyDescriptor("prop1", beanClass);
            prop1.setBound(true);
            prop1.setDisplayName("Property 1");

            PropertyDescriptor rv[] = {prop1, prop2};
            return rv;
        }
        catch (IntrospectionException e) {
            throw new Error(e.toString());
        }
*/
        return null;
    }

    public MethodDescriptor[] getMethodDescriptors() {
        try {
            Method mthd;

            //showDialog requests that the AddressList display its file dialog.
            //public void showDialog () {
            mthd = beanClass.getMethod("showDialog");
            MethodDescriptor showDialogDesc = new MethodDescriptor (mthd);
            showDialogDesc.setShortDescription ("Displays the file dialog");

            //Sets this gadgets enabled state.
            //public void setEnabled (boolean aEnabled) {
            ParameterDescriptor setEnabledPD = new ParameterDescriptor ();
            setEnabledPD.setShortDescription ("Boolean value for enabled");
            ParameterDescriptor[] setEnabledPDA = { setEnabledPD };

            Class[] setEnabledParams = { java.lang.Boolean.TYPE };
            mthd = beanClass.getMethod ("setEnabled", setEnabledParams);
            MethodDescriptor setEnabledDesc = new MethodDescriptor (mthd, setEnabledPDA);
            setEnabledDesc.setShortDescription ("Sets this gadgets enabled state");

            //Delets all attachments from the list.
            //public void removeAllAttachments () {
            mthd = beanClass.getMethod ("removeAllAttachments");
            MethodDescriptor removeAllAttachmentsDesc = new MethodDescriptor (mthd);
            removeAllAttachmentsDesc.setShortDescription ("Delets all attachments from the list");

            //Remove an entry at index from the list.
            //public void removeAttachment (int aIndex) {
            ParameterDescriptor removeAttachmentPD = new ParameterDescriptor ();
            removeAttachmentPD.setShortDescription ("The index of the attachment to remove");
            ParameterDescriptor[] removeAttachmentPDA = { removeAttachmentPD };

            Class[] removeAttachmentParams = { java.lang.Integer.TYPE };
            mthd = beanClass.getMethod ("removeAttachment", removeAttachmentParams);
            MethodDescriptor removeAttachmentDesc = new MethodDescriptor (mthd, removeAttachmentPDA);
            removeAttachmentDesc.setShortDescription ("Remove an entry at index from the list");

            //Adds an file to the list.
            //public void addAttachment (String aFilePath) {
            ParameterDescriptor addAttachmentPD = new ParameterDescriptor ();
            addAttachmentPD.setShortDescription ("A file path");
            ParameterDescriptor[] addAttachmentPDA = { addAttachmentPD };

            Class[] addAttachmentParams = { String.class };
            mthd = beanClass.getMethod ("addAttachment", addAttachmentParams);
            MethodDescriptor addAttachmentDesc = new MethodDescriptor (mthd, addAttachmentPDA);
            addAttachmentDesc.setShortDescription ("Adds an file to the list");

            //pull it all together now.
            MethodDescriptor [] mda = { showDialogDesc, setEnabledDesc, removeAllAttachmentsDesc, removeAttachmentDesc, addAttachmentDesc };            return mda;
         }
         catch (SecurityException e) {
            return null;
         }
         catch (NoSuchMethodException e) {
            return null;
         }
    }
}
