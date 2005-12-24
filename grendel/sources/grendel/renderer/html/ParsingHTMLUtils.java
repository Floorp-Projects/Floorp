/*
 * HTMLtoPlain.java
 *
 * Created on 24 August 2005, 20:44
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.renderer.html;
import java.io.BufferedReader;
import java.io.CharArrayReader;
import java.io.IOException;
import org.htmlparser.Node;
import org.htmlparser.NodeFilter;
import org.htmlparser.filters.HasParentFilter;
import org.htmlparser.filters.TagNameFilter;
import org.htmlparser.util.NodeIterator;
import org.htmlparser.util.NodeList;
import org.htmlparser.util.ParserException;
import org.htmlparser.util.SimpleNodeIterator;



/**
 *
 * @author hash9
 */
public class ParsingHTMLUtils extends Thread {
    
    public static String clean(String s) throws ParserException {
        org.htmlparser.Parser p = org.htmlparser.Parser.createParser(s,"ISO-8859-1");
        /*NodeList nl_strange = p.extractAllNodesThatMatch(new HasParentFilter(new TagNameFilter("body"),true));
        ArrayList<Node> list_nodes = new ArrayList<Node>();
        SimpleNodeIterator sni = nl_strange.elements();
        NodeList nl = new NodeList();
        for (Node n = sni.nextNode();sni.hasMoreNodes();n=sni.nextNode()) {
            Node parent = n.getParent();
            boolean found = false;
            while (parent!=null) {
                if (list_nodes.contains(n)) {
                    found=true;
                    break;
                }
                parent = parent.getParent();
            }
            if (! found) {
                nl.add(n);
                list_nodes.add(n);
            }
        }
        try {
            StringBuilder sb = new StringBuilder();
            BufferedReader br = new BufferedReader(new CharArrayReader(nl.toHtml().toCharArray()));
            while (br.ready()) {
                String line = br.readLine().trim();
                if (!line.equals("")) {
                    sb.append(line);
                    sb.append('\n');
                }
            }
            return sb.toString().trim();
        } catch (IOException ioe) {
            ioe.printStackTrace();
            return nl.toHtml().trim();
        }*/
        //NodeFilter nf = new HasParentFilter(new TagNameFilter("body"),true);
        NodeFilter nf = new TagNameFilter("body");
        NodeIterator ni = p.elements();
        Node root = ni.nextNode();
        if (root == null) return s;
        NodeList childern = root.getChildren();
        if (childern == null) return s;
        SimpleNodeIterator sni = childern.elements();
        for (Node n=sni.nextNode();sni.hasMoreNodes();n=sni.nextNode()) {
            if (nf.accept(n)) {
                //System.out.println(n.toHtml());
                StringBuilder sb = new StringBuilder();
                SimpleNodeIterator ssni = n.getChildren().elements();
                for (Node sn=ssni.nextNode();ssni.hasMoreNodes();sn=ssni.nextNode()) {
                    try {
                        BufferedReader br = new BufferedReader(new CharArrayReader(sn.toHtml().trim().toCharArray()));
                        while (br.ready()) {
                            String line = br.readLine().trim();
                            if (!line.equals("")) {
                                sb.append(line);
                                sb.append('\n');
                            }
                        }
                    } catch (IOException ioe) {
                        ioe.printStackTrace();
                        sb.append(sn.toHtml().trim());
                        sb.append('\n');
                    }
                }
                return sb.toString().trim();
            }
            //System.out.println("node");
            
        }
        return "";
    }
    public static String cleanTidy(String s) throws ParserException {
        return tidy(clean(s));
    }
    
    public static String tidy(String s) throws ParserException {
        StringBuilder sb = new StringBuilder();
        org.htmlparser.lexer.Lexer l = new org.htmlparser.lexer.Lexer(s);
        //org.htmlparser.Parser p = org.htmlparser.Parser.createParser(s,"ISO-8859-1");
        
        Node n = null;
        do {
            n= l.nextNode(true);
            if (n!=null) {
                String current =  n.toHtml().trim();
                if (! current.equals("")) {
                    sb.append(current);
                    sb.append('\n');
                }
            }
        } while (n!=null);
        
        
        /*StringBuilder lcase = new StringBuilder(s.toLowerCase());
        StringBuilder content = new StringBuilder(s.toLowerCase());*/
        
        return sb.toString();
    }
    
    public static String toText(String s) throws ParserException {
        s = clean(s);
        s = tidy(s);
        
        NodeFilter table = new TagNameFilter("table");
        NodeFilter table_element = new HasParentFilter(new TagNameFilter("table"));
        NodeFilter tr = new TagNameFilter("tr");
        NodeFilter td = new TagNameFilter("td");
        
        StringBuilder sb = new StringBuilder();
        org.htmlparser.lexer.Lexer l = new org.htmlparser.lexer.Lexer(s);
        //org.htmlparser.Parser p = org.htmlparser.Parser.createParser(s,"ISO-8859-1");
        
        Node n = null;
        int nestled = 0;
        do {
            n= l.nextNode(true);
            if (n!=null) {
                String string = n.toHtml();
                String current =  n.toPlainTextString();
                
                if (string.toLowerCase().startsWith("<table")) {
                    nestled++;
                }
                if (string.equalsIgnoreCase("</table>")) {
                    if (nestled == 1) {
                        sb.append('\n');
                    }
                    nestled--;
                }
                
                if (string.equalsIgnoreCase("</td>")) {
                    sb.append('\t');
                } else if (string.equalsIgnoreCase("</tr>")) {
                    if (sb.lastIndexOf("\t")==(sb.length()-1)) {
                        sb.setCharAt((sb.length()-1),'\n');
                    } else {
                        sb.append('\n');
                    }
                } else {
                    if (current.trim().equals("")) {
                        int index = current.indexOf('\n');
                        if (index>0&&index<current.length()-1) {
                            sb.append('\n');
                        }
                    } else {
                        sb.append(current.trim());
                        if (nestled<1) {
                            sb.append('\n');
                        }
                    }
                }
            }
        } while (n!=null);
        
        
        /*StringBuilder lcase = new StringBuilder(s.toLowerCase());
        StringBuilder content = new StringBuilder(s.toLowerCase());*/
        
        return sb.toString();
    }
    
    public static void main(String... args) throws ParserException {
        StringBuilder sb = new StringBuilder();
        sb.append("<html>\n");
        sb.append("<head>\n");
        sb.append("<title>Re: Test Maildir</title>\n");
        sb.append("<link rel=\"important stylesheet\" href=\"chrome://messenger/skin/messageBody.css\">\n");
        sb.append("</head>\n");
        sb.append("<body>\n");
        sb.append("<table border=0 cellspacing=0 cellpadding=0 width=\"100%\" class=\"header-part1\"><tr><td><b>Subject: </b>Re: Test Maildir</td></tr><tr><td><b>From: </b>Hash9 Grendel <hash9@res04-kam58.res.st-and.ac.uk></td></tr><tr><td><b>Date: </b>Wed, 24 Aug 2005 15:20:09 +0100 (BST)</td></tr>\n");
        sb.append("<tr><td><b>To: </b>Hash9 Grendel <hash9@res04-kam58.res.st-and.ac.uk></td></tr><tr><td><b>CC: </b>postmaster@res04-kam58.res.st-and.ac.uk</td></tr></table><br>\n");
        sb.append("On Wed, 2005-08-24, at 15:13:48 +0100, Hash9 Grendel wrote:<br>\n");
        sb.append("<blockquote type=\"cite\"><html>\n");
        sb.append("<head>\n");
        sb.append("</head>\n");
        sb.append("<body>\n");
        sb.append("\n");
        sb.append("<div>\n");
        sb.append("On Wed, 2005-08-24, at 15:11:57 +0100, Hash9 Grendel wrote:<br>\n");
        sb.append("<blockquote type=\"cite\">\n");
        sb.append("<head>\n");
        sb.append("</head>\n");
        sb.append("\n");
        sb.append("\n");
        sb.append("<div>\n");
        sb.append("On Wed, 2005-08-24, at 14:51:09 +0100, Hash9 Grendel wrote:<br>\n");
        sb.append("<blockquote type=\"cite\">\n");
        sb.append("<head>\n");
        sb.append("</head>\n");
        sb.append("\n");
        sb.append("\n");
        sb.append("<div>\n");
        sb.append("On Wed, 2005-08-24, at 14:50:05 +0100, Hash9 wrote:<br>\n");
        sb.append("<blockquote type=\"cite\">\n");
        sb.append("<head>\n");
        sb.append("</head>\n");
        sb.append("\n");
        sb.append("Hash9 Grendel wrote:\n");
        sb.append("<br>\n");
        sb.append("<br>> On Sun, 2005-04-24, at 12:49:21 +0100, \"Upsilon\" wrote:\n");
        sb.append("<br>>\n");
        sb.append("<br>>> Testing maildir delivery.\n");
        sb.append("<br>>>\n");
        sb.append("<br>>> Test\n");
        sb.append("<br>>> ------------------------------------------------------------------------\n");
        sb.append("<br>>>\n");
        sb.append("<br>>> Testing maildir delivery.\n");
        sb.append("<br>>>  \n");
        sb.append("<br>>> Test\n");
        sb.append("<br>>\n");
        sb.append("<br>\n");
        sb.append("<br>\n");
        sb.append("\n");
        sb.append("</blockquote>\n");
        sb.append("\n");
        sb.append("</div>\n");
        sb.append("</blockquote>\n");
        sb.append("\n");
        sb.append("</div>\n");
        sb.append("</blockquote>\n");
        sb.append("\n");
        sb.append("</div>\n");
        sb.append("</body>\n");
        sb.append("</html>\n");
        sb.append("</blockquote>\n");
        sb.append("</body>\n");
        sb.append("</html>\n");
        
        System.out.println(toText(sb.toString()));
    }
}
