/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * RenderMessage.java
 *
 * Created on 07 August 2005, 18:25
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
package grendel.renderer.tools;

import grendel.renderer.ObjectRender;
import grendel.renderer.html.BriefHeaderFormatter;
import grendel.renderer.html.FullHeaderFormatter;
import grendel.renderer.html.NormalHeaderFormatter;

import java.io.IOException;
import java.util.Enumeration;
import java.util.List;

import javax.mail.Header;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Part;
import javax.mail.internet.InternetHeaders;


/**
 *
 * @author hash9
 */
public class RenderMessage implements ObjectRender {
    private static List<String> l=null;
    
    /** Creates a new instance of RenderMessage */
    public RenderMessage() {
    }
    
    public boolean accept(Object o) {
        if (o instanceof Message) {
            return true;
        } else {
            return false;
        }
    }
    
    public Class acceptable() {
        return Message.class;
    }
    
    public StringBuilder objectRenderer(
            Object o, String index, Part p, grendel.renderer.Renderer master)
            throws MessagingException, IOException {
        Message message=(Message) p;
        StringBuilder buf=new StringBuilder();
        boolean indent=true;
        
        if (index==null) {
            indent=false;
            index="";
        }
        
        if (indent) {
            buf.append("<blockquote>");
            buf.append("<table border=\"1\" cellspacing=\"0\">");
            buf.append("<tr><td>");
        }
        
        if (! master.isReply()){
            InternetHeaders ih=new InternetHeaders();
            Enumeration enumm=p.getAllHeaders();
            
            while (enumm.hasMoreElements()) {
                Header h=(Header) enumm.nextElement();
                ih.addHeader(h.getName(), h.getValue());
            }
            
            //TODO Add an option to select which of these to use.
        /*new BriefHeaderFormatter().formatHeaders(ih, buf);
        buf.append("<hr>");*/
            new NormalHeaderFormatter().formatHeaders(ih, buf);
            buf.append("<hr>");
        /*new FullHeaderFormatter().formatHeaders(ih, buf);
        buf.append("<hr>");*/
        }
        Object o_1=message.getContent();
        buf.append(master.objectRenderer(o_1, index, message));
        
        if (indent) {
            buf.append("</td></tr>");
            buf.append("</table>");
            buf.append("</blockquote>");
        }
        
        return buf;
    }
}
