package com.netscape.jsdebugging.remote.corba;
abstract public class IJSExecutionHookHelper {
  public static com.netscape.jsdebugging.remote.corba.IJSExecutionHook narrow(org.omg.CORBA.Object object) {
    return narrow(object, false);
  }
  private static com.netscape.jsdebugging.remote.corba.IJSExecutionHook narrow(org.omg.CORBA.Object object, boolean is_a) {
    if(object == null) {
      return null;
    }
    if(object instanceof com.netscape.jsdebugging.remote.corba.IJSExecutionHook) {
      return (com.netscape.jsdebugging.remote.corba.IJSExecutionHook) object;
    }
    if(is_a || object._is_a(id())) {
      com.netscape.jsdebugging.remote.corba.IJSExecutionHook result = new com.netscape.jsdebugging.remote.corba._st_IJSExecutionHook();
      ((org.omg.CORBA.portable.ObjectImpl) result)._set_delegate
        (((org.omg.CORBA.portable.ObjectImpl) object)._get_delegate());
      return result;
    }
    return null;
  }
  public static com.netscape.jsdebugging.remote.corba.IJSExecutionHook bind(org.omg.CORBA.ORB orb) {
    return bind(orb, null, null, null);
  }
  public static com.netscape.jsdebugging.remote.corba.IJSExecutionHook bind(org.omg.CORBA.ORB orb, java.lang.String name) {
    return bind(orb, name, null, null);
  }
  public static com.netscape.jsdebugging.remote.corba.IJSExecutionHook bind(org.omg.CORBA.ORB orb, java.lang.String name, java.lang.String host, org.omg.CORBA.BindOptions options) {
    return narrow(orb.bind(id(), name, host, options), true);
  }
  private static org.omg.CORBA.ORB _orb() {
    return org.omg.CORBA.ORB.init();
  }
  public static com.netscape.jsdebugging.remote.corba.IJSExecutionHook read(org.omg.CORBA.portable.InputStream _input) {
    return com.netscape.jsdebugging.remote.corba.IJSExecutionHookHelper.narrow(_input.read_Object(), true);
  }
  public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.IJSExecutionHook value) {
    _output.write_Object(value);
  }
  public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.IJSExecutionHook value) {
    org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
    write(output, value);
    any.read_value(output.create_input_stream(), type());
  }
  public static com.netscape.jsdebugging.remote.corba.IJSExecutionHook extract(org.omg.CORBA.Any any) {
    if(!any.type().equal(type())) {
      throw new org.omg.CORBA.BAD_TYPECODE();
    }
    return read(any.create_input_stream());
  }
  private static org.omg.CORBA.TypeCode _type;
  public static org.omg.CORBA.TypeCode type() {
    if(_type == null) {
      _type = _orb().create_interface_tc(id(), "IJSExecutionHook");
    }
    return _type;
  }
  public static java.lang.String id() {
    return "IDL:IJSExecutionHook:1.0";
  }
}
