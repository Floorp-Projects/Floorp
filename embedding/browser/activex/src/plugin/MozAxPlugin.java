import netscape.plugin.Plugin;

class MozAxPlugin extends Plugin
{
    native  Object GetProperty(String dispid);
    native  void SetProperty(String dispid, String Property);
    native  void SetProperty(String dispid, Object property);

    native  Object Invoke(String dispid);
    native  Object Invoke(String dispid, Object param1);
    native  Object Invoke(String dispid, Object param1, Object param2);
    native  Object Invoke(String dispid, Object param1, Object param2, Object param3);
    native  Object Invoke(String dispid, Object param1, Object param2, Object param3, Object param4);
};