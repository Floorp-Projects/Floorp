import netscape.plugin.Plugin;

class MozAxPlugin extends Plugin
{
    native  Object getProperty(String dispid);
    native  void setProperty(String dispid, String property);
    native  void setProperty(String dispid, Object property);

    native  Object invoke(String dispid);
    native  Object invoke(String dispid, Object param1);
    native  Object invoke(String dispid, Object param1, Object param2);
    native  Object invoke(String dispid, Object param1, Object param2, Object param3);
    native  Object invoke(String dispid, Object param1, Object param2, Object param3, Object param4);
};