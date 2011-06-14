import netscape.plugin.Plugin;

public class MozAxPlugin extends Plugin
{
    ///////////////////////////////////////////////////////////////////////////
    // Public methods that are exposed through LiveConnect

    public Object getProperty(String dispid)
    {
      return xgetProperty(dispid);
    }
    public void setProperty(String dispid, String property)
    {
      xsetProperty1(dispid, property);
    }
    public void setProperty(String dispid, Object property)
    {
      xsetProperty2(dispid, property);
    }

    public Object invoke(String dispid)
    {
      return xinvoke(dispid);
    }

    public Object invoke(String dispid, Object p1)
    {
      return xinvoke1(dispid, p1);
    }
    public Object invoke(String dispid, Object p1, Object p2)
    {
      return xinvoke2(dispid, p1, p2);
    }
    public Object invoke(String dispid, Object p1, Object p2, Object p3)
    {
      return xinvoke3(dispid, p1, p2, p3);
    }
    public Object invoke(String dispid, Object p1, Object p2, Object p3, Object p4)
    {
      return xinvoke4(dispid, p1, p2, p3, p4);
    }
    public Object invoke(String dispid, Object params[])
    {
      return xinvokeX(dispid, params);
    }

    ///////////////////////////////////////////////////////////////////////////
    // Native implementations of the above methods.
    //
    // Note: These methods are not overloaded like the public versions above
    //       because javah generates bad code which doesn't compile if you try.

    private native Object xgetProperty(String dispid);
    private native void xsetProperty1(String dispid, String property);
    private native void xsetProperty2(String dispid, Object property);
    private native Object xinvoke(String dispid);
    private native Object xinvoke1(String dispid, Object p1);
    private native Object xinvoke2(String dispid, Object p1, Object p2);
    private native Object xinvoke3(String dispid, Object p1, Object p2, Object p3);
    private native Object xinvoke4(String dispid, Object p1, Object p2, Object p3, Object p4);
    private native Object xinvokeX(String dispid, Object params[]);
};
