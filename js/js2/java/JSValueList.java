
import java.util.Vector;

class JSValueList extends JSValue {
    
    static JSValueList buildList(JSValue left, JSValue right)
    {
        JSValueList theList;
        if (left instanceof JSValueList) {
            theList = (JSValueList)left;
            theList.add(right);
        }
        else
            if (right instanceof JSValueList) {
                theList = (JSValueList)right;
                theList.add(left);
            }
            else {
                theList = new JSValueList();
                theList.add(left);
                theList.add(right);
            }
        
        return theList;                       
    }
    
    void add(JSValue v)
    {
        if (v instanceof JSValueList) {
            JSValueList vl = (JSValueList)v;
            for (int i = 0; i < vl.contents.size(); i++)
                contents.addElement((JSValue)(vl.contents.elementAt(i)));
        }
        else
            contents.addElement(v);
    }
    
    Vector contents = new Vector();
    
}