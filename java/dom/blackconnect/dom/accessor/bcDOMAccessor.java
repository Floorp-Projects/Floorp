
import org.mozilla.xpcom.*;
import org.mozilla.dom.Attr;
import org.mozilla.dom.Node;
import org.mozilla.dom.Document;
import org.mozilla.dom.Element;
import org.mozilla.dom.DOMTreeDumper;


public class bcDOMAccessor implements bcIDOMAccessor {
    public bcDOMAccessor() {}
    public Object queryInterface(IID iid) {
        System.out.println("--[java]::queryInterface iid="+iid);
        Object result;
        if ( iid.equals(nsISupportsIID)
             || iid.equals(bcIDOMAccessorIID)) {
            result = this;
        } else {
            result = null;
        }
        System.out.println("--[java]::queryInterface result=null "+(result==null));
        return result;
    }
  
    public void endDocumentLoad(String url, Document doc) {
	System.out.println("=== GOT URL: " + url);
	if (url.startsWith("about")) {
	    return;
	}
	new DOMTreeDumper().dumpToStream(System.out, doc);

    }

    static IID bcIDOMAccessorIID = new IID(bcIDOMAccessor.IID);
    static IID nsISupportsIID = new IID(nsISupports.IID);
    static {
         InterfaceRegistry.register("org.mozilla.dom.Attr");
         InterfaceRegistry.register("org.mozilla.dom.CDATASection");
         InterfaceRegistry.register("org.mozilla.dom.CharacterData");
         InterfaceRegistry.register("org.mozilla.dom.Comment");
         InterfaceRegistry.register("org.mozilla.dom.Document");
         InterfaceRegistry.register("org.mozilla.dom.DocumentFragment");
         InterfaceRegistry.register("org.mozilla.dom.DocumentType");
         InterfaceRegistry.register("org.mozilla.dom.DOMImplementation");
         InterfaceRegistry.register("org.mozilla.dom.Element");
         InterfaceRegistry.register("org.mozilla.dom.Entity");
         InterfaceRegistry.register("org.mozilla.dom.EntityReference");
         InterfaceRegistry.register("org.mozilla.dom.NamedNodeMap");
         InterfaceRegistry.register("org.mozilla.dom.Node");
         InterfaceRegistry.register("org.mozilla.dom.NodeList");
         InterfaceRegistry.register("org.mozilla.dom.Notation");
         InterfaceRegistry.register("org.mozilla.dom.ProcessingInstruction");
         InterfaceRegistry.register("org.mozilla.dom.Text");    
    }
}

