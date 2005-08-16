/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * HTMLUtils.java
 *
 * Created on 07 August 2005, 18:32
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
package grendel.renderer;

import grendel.mime.html.TextHTMLConverter;


/**
 *
 * @author hash9
 */
public final class HTMLUtils
{
  /** Creates a new instance of HTMLUtils */
  private HTMLUtils()
  {
  }

  public static String cleanHTML(String s)
  {
    StringBuilder sb=new StringBuilder();
    s=s.replace("<HTML>", "");
    s=s.replace("</HTML>", "");
    s=s.replace("<html>", "");
    s=s.replace("</html>", "");
    s=s.replace("<BODY>", "");
    s=s.replace("</BODY>", "");
    s=s.replace("<body>", "");
    s=s.replace("</body>", "");
    sb.append("\n<div>\n");

    /*sb.append("<object type=\"text/html\">\n");
    sb.append(s);
    sb.append("\n</object>\n");*/

    //sb.append("\n<iframe>\n");
    sb.append(s);

    //sb.append("\n</iframe>\n");
    sb.append("\n</div>\n");

    return sb.toString();
  }

  public static String genHRef(String name, String url)
  {
    StringBuilder sb=new StringBuilder();
    sb.append(genHRefStart(url));
    sb.append(name);
    sb.append("</a>");

    return sb.toString();
  }

  public static String genHRefStart(String url)
  {
    StringBuilder sb=new StringBuilder();
    sb.append("<a href=\"");
    sb.append(url);
    sb.append("\">");

    return sb.toString();
  }

  public static String genRow(String name, String value, String url)
  {
    String value_q=quoteToHTML(value);
    String value_a=genHRef(value_q, url);

    return genRow(name, value_a, false);
  }

  public static String genRow(String name, String value)
  {
    return genRow(name, value, true);
  }

  public static String genRow(String name, String value, boolean quote)
  {
    if (quote) {
      value=quoteToHTML(value);
    }

    StringBuilder sb=new StringBuilder();
    sb.append("<tr>\n");
    sb.append("<td align=\"right\"><b>");
    sb.append(name);
    sb.append(":");
    sb.append("</b></td>\n");
    sb.append("<td>");
    sb.append(value);
    sb.append("</td>\n");
    sb.append("</tr>\n");

    return sb.toString();
  }

  public static String quoteToHTML(String s)
  {
    StringBuffer buffer=new StringBuffer(s);
    TextHTMLConverter.quoteForHTML(buffer, true, true);

    String strData=buffer.toString();

    //strData = strData.replace(" ", "&nbsp;");
    strData=strData.replace("\n", "<br>");

    return strData;
  }
}
