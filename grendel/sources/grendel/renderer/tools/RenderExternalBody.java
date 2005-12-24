/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL
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
 * Created: Jamie Zawinski <jwz@netscape.com>, 28 Aug 1997.
 */
package grendel.renderer.tools;

import calypso.util.ByteBuf;

import grendel.renderer.ObjectRender;

import java.io.PrintStream;

import javax.mail.internet.InternetHeaders;


/** This class converts the message/external-body  MIME type to HTML
 * that points at the remote document.
 */
class RenderExternalBody implements ObjectRender {
    
    public boolean accept(Object o) {
        return false;
    }
    
    public Class acceptable() {
        return javax.mail.Address.class;
    }
    
    public StringBuilder objectRenderer(Object o, String index, javax.mail.Part p, grendel.renderer.Renderer master) throws javax.mail.MessagingException, java.io.IOException {
       return new StringBuilder();
    }
        /*
        ByteBuf line=new ByteBuf();
        
        // silly constructed headers used for display purposes
        InternetHeaders dpy_headers=new InternetHeaders();
        
        String ct="";
        String[] s_a = p.getHeader("Content-Type");
        if ((s_a != null) && (s_a.length>0)) {
            ct = s_a[0];
        }
        
        make_header("Access-Type", ct, headers, dpy_headers);
        make_header("URL", ct, headers, dpy_headers);
        make_header("Site", ct, headers, dpy_headers);
        make_header("Server", ct, headers, dpy_headers);
        make_header("Directory", ct, headers, dpy_headers);
        make_header("Name", ct, headers, dpy_headers);
        
        // the *inner* content-type
        if ((body!=null)&&(body.headers!=null)) {
            String ict=getHeaderValue("Content-Type", body.headers, true);
            
            if (ict!=null) {
                line.insert(0, "Type: ");
                dpy_headers.addHeaderLine(ict);
            }
        }
        
        make_header("Size", ct, headers, dpy_headers);
        make_header("Expiration", ct, headers, dpy_headers);
        make_header("Permission", ct, headers, dpy_headers);
        make_header("Mode", ct, headers, dpy_headers);
        make_header("Subject", ct, headers, dpy_headers);
        
        String body_str=(((body!=null)&&(body.buffer!=null))
        ? body.buffer.toString() : null);
        
        if (body_str!=null) {
            body_str=body_str.trim();
            
            if (body_str.length()==0) {
                body_str=null;
            }
        }
        
        String url=makeURL(dpy_headers, body_str);
        String lname=((url==null) ? "Document Info" // #### I18N
                : "Link to Document"); // #### I18N
        
        formatAttachmentBox(dpy_headers, null, lname, url, body_str, true);
        dpy_headers=null;
        body=null;
    }
    
    String makeURL(InternetHeaders headers, String body) {
        String at=getHeaderValue("Access-Type", headers, true);
        
        if (at==null) {
            return null;
        } else if (at.equalsIgnoreCase("ftp")||at.equalsIgnoreCase("anon-ftp")) {
            String site=getHeaderValue("Site", headers, true);
            String dir=getHeaderValue("Directory", headers, true);
            String name=getHeaderValue("name", headers, true);
            
            if (site==null) {
                return null;
            } else {
                String result="ftp://"+site+"/";
                
                if (dir!=null) {
                    if (dir.charAt(0)=='/') {
                        dir=dir.substring(1);
                    }
                    
                    result+=dir;
                    
                    if (result.charAt(result.length()-1)!='/') {
                        result+="/";
                    }
                }
                
                if (name!=null) {
                    result+=name;
                }
                
                return result;
            }
        } else if (at.equalsIgnoreCase("local-file")||at.equalsIgnoreCase("afs")) {
            String name=getHeaderValue("Name", headers, true);
            
            if (name==null) {
                return null;
            }
            
            // name = NET_Escape(name, URL_PATH); // ####
            return "file:"+name;
        } else if (at.equalsIgnoreCase("mail-server")) {
            String server=getHeaderValue("Server", headers, true);
            String subject=getHeaderValue("Subject", headers, false);
            
            if (server==null) {
                return null;
            }
            
            // server = NET_Escape(server, URL_XALPHAS);    // ####
            // subject = NET_Escape(subject, URL_XALPHAS);  // ####
            // body = NET_Escape(body, URL_XALPHAS);        // ####
            String result="mailto:"+server;
            
            if (subject!=null) {
                result+=("?subject="+subject);
            }
            
            if (body!=null) {
                if (subject!=null) {
                    result+="&";
                } else {
                    result+="?";
                }
                
                result+=("body="+body);
            }
            
            return result;
        } else if (at.equalsIgnoreCase("url")) { // RFC 2017
            
            return getHeaderValue("URL", headers, false);
        } else {
            return null;
        }
    }
    
    void make_header(
            String param_name, String header_value, InternetHeaders input_headers,
            InternetHeaders output_headers) {
        ByteBuf scratch=new ByteBuf();
        
        //####
        //    ByteBuf bb = new ByteBuf(header_value);
        //    if (input_headers.getParameter(bb, param_name, scratch)) {
        //
        //      // Kludge for URLs...  if this is a URL parameter, remove all whitespace
        //      // from it. (The URL parameter to one of these headers is stored with
        //      // lines broken every 40 characters or less; it's assumed that all
        //      // significant whitespace was URL-hex-encoded, and all the rest of it
        //      // was inserted just to keep the lines short.)
        //      if (param_name.equalsIgnoreCase("URL")) {
        //        int i = 0;
        //        while (i < scratch.length()) {
        //          if ((scratch.toBytes())[i] <= ' ')
        //            scratch.remove(i, i+1);
        //          else
        //            i++;
        //        }
        //      }
        //
        //      scratch.insert(0, param_name + ": ");
        //      output_headers.addLine(scratch);
        //    }
    }
    
    void writeBoxEarly() {
    } // we need to wait for EOF to write the box.
    
    private String getHeaderValue(String name, InternetHeaders ih, boolean strip) {
        String[] hh=ih.getHeader(name);
        
        if ((hh==null)||(hh.length==0)) {
            return null;
        }
        
        String v=hh[0];
        
        // #### handle strip
        return v;
    }*/
}
