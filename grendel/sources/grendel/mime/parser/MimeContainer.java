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
 * Created: Jamie Zawinski <jwz@netscape.com>, 21 Aug 1997.
 */

package grendel.mime.parser;


import grendel.mime.IMimeOperator;
import calypso.util.ByteBuf;

import javax.mail.internet.InternetHeaders;

import java.util.Enumeration;
import java.util.Vector;
import java.util.StringTokenizer;

//import java.lang.reflect.Constructor;
//import java.lang.reflect.InvocationTargetException;


/** This abstract class represents the parsers for all MIME objects which can
    contain other MIME objects within them.
 */
abstract class MimeContainer extends MimeObject {

  Vector kids = null;
  MimeObject open_child = null;

  public MimeContainer(String content_type, InternetHeaders headers) {
    super(content_type, headers);
  }

  public Enumeration children() { return kids.elements(); }

  /** This method creates a new child part, making the decision about which
      MIME content-type strings correspond to which subclasses of MimeObject.
      It does this by mapping the type name to a class name, and attempting
      to load it from the `grendel.mime.parser' package.
   */
  public MimeObject makeChild(String child_type,
                              InternetHeaders child_headers) {

    if (child_type == null && child_headers != null) {
      String hh[] = child_headers.getHeader("Content-Type");
      if (hh != null && hh.length != 0) {
        child_type = hh[0];
        // #### strip to first token
        int i = child_type.indexOf(';');
        if (i > 0)
          child_type = child_type.substring(0, i);
        child_type = child_type.trim();
      }
    }

    if (child_type == null)
      return new MimeDwimText(child_type, child_headers);

    else if (child_type.equalsIgnoreCase("message/rfc822") ||
             child_type.equalsIgnoreCase("message/news"))
      return new MimeMessageRFC822(child_type, child_headers);

    else if (child_type.equalsIgnoreCase("message/external-body"))
      return new MimeMessageExternalBody(child_type, child_headers);

    else if (child_type.equalsIgnoreCase("multipart/digest"))
      return new MimeMultipartDigest(child_type, child_headers);

    else if (child_type.regionMatches(true, 0, "multipart/", 0, 10))
      return new MimeMultipart(child_type, child_headers);

    else if (child_type.equalsIgnoreCase("x-sun-attachment"))
      return new MimeXSunAttachment(child_type, child_headers);
    else
      return new MimeLeaf(child_type, child_headers);
  }


  /* This hairy mess does the same thing as the above version, but it
     does so by dynamically looking up classes at runtime: if it sees
     a MIME type of "multipart/foobar-baz", it would try to instantiate,
     in order,

        grendel.mime.parser.MimeMultipartFoobarBaz
        grendel.mime.parser.MimeMultipart
        grendel.mime.parser.Leaf

     This is all well and good, but it turns out to be a bit slower, and
     I'm not convinced this generality is actually useful here in the
     parser, so I'm going to punt on it for now.
   */
//  public MimeObject makeChild(String child_type, InternetHeaders child_headers) {
//
//    if (child_type == null && child_headers != null)
//      child_type = child_headers.getHeaderValue("Content-Type", true, false);
//
//    String pkg = "grendel.mime.parser";
//    String base_class_name = null;
//    String full_class_name = null;
//
//    if (child_type == null) {
//      full_class_name = "MimeDwimText";
//    } else {
//
//      if (child_type.equalsIgnoreCase("message/news"))  // synonym kludge
//        child_type = "message/rfc822";
//      else if (child_type.equalsIgnoreCase("multipart/header-set"))
//        child_type = "multipart/appledouble";
//
//      StringTokenizer st = new StringTokenizer(child_type, " /-_", false);
//      while (st.hasMoreTokens()) {
//        String t = st.nextToken();
//        t = (String.valueOf(Character.toUpperCase(t.charAt(0))) +
//             t.substring(1).toLowerCase());
//        if (t.equals("Rfc822")) t = "RFC822";  // typographical kludge
//        if (base_class_name == null)
//          base_class_name = "Mime" + t;
//        if (full_class_name == null)
//          full_class_name = "Mime";
//        full_class_name += t;
//      }
//    }
//
//    if (full_class_name != null)
//      full_class_name = pkg + "." + full_class_name;
//    if (base_class_name != null)
//      base_class_name = pkg + "." + base_class_name;
//
//
//    Class types[] = { "".getClass(), child_headers.getClass() };
//    Object args[] = { child_type, child_headers };
//    MimeObject result;
//
//    // First try to find and instantiate the full-class version...
//    result = try_class(full_class_name, types, args);
//    if (result != null) return result;
//
//    // If that failed, try to find and instantiate the base-class version.
//    result = try_class(base_class_name, types, args);
//    if (result != null) return result;
//
//    // If that failed, use the leaf class (this better work.)
//    result = try_class(pkg + ".MimeLeaf", types, args);
//    if (result == null) throw new Error("internal error: no classes");
//    return result;
//  }
//
//  // Tries to find a class of the given name, and construct an instance
//  // with a two-arg constructor (String, InternetHeaders).  Returns null if either
//  // fails.
//  private final MimeObject try_class(String class_name,
//                                     Class[] types, Object[] args) {
//    if (class_name == null) return null;
//    try {
//      Class c = Class.forName(class_name);
//
//      Constructor cc = c.getConstructor(types);
//      return (MimeObject) cc.newInstance(args);
//
//    } catch (ClassNotFoundException c) {        // Class.forName
//    } catch (NoSuchMethodException c) {         // Constructor.getConstructor
//    } catch (IllegalAccessException c) {        // Constructor.newInstance
//    } catch (IllegalArgumentException c) {      // Constructor.newInstance
//    } catch (InstantiationException c) {        // Constructor.newInstance
//    } catch (InvocationTargetException c) {     // Constructor.newInstance
//    }
//    return null;
//  }


  /** This method creates a new child part (via makeChild()), and then
      creates an operator for it, and installs it in `kids'.
   */
  public MimeObject openChild(String child_type,
                              InternetHeaders child_headers) {
    if (open_child != null)
      closeChild();
    open_child = makeChild(child_type, child_headers);

    if (kids == null)
      kids = new Vector();

    if (id == null)
      open_child.id = String.valueOf(kids.size() + 1);
    else
      open_child.id = id + "." + String.valueOf(kids.size() + 1);

    open_child.setOperator(operator.createChild(open_child));
    kids.addElement(open_child);
    return open_child;
  }

  public void closeChild() {
    if (open_child != null) {
      open_child.pushEOF();
      open_child = null;
    }
  }

  public MimeObject openChild(ByteBuf child_type,
                              InternetHeaders child_headers) {
    return openChild((child_type == null
                      ? null
                      : child_type.toString()),
                     child_headers);
  }

  public MimeObject openChild(byte child_type[],
                              InternetHeaders child_headers) {
    return openChild((child_type == null
                      ? null
                      : new String(child_type)),
                     child_headers);
  }

  public void pushEOF() {
    closeChild();
    super.pushEOF();
  }
}
