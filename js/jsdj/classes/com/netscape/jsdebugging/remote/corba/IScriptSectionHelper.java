package com.netscape.jsdebugging.remote.corba;
abstract public class IScriptSectionHelper {
  private static org.omg.CORBA.ORB _orb() {
    return org.omg.CORBA.ORB.init();
  }
  public static com.netscape.jsdebugging.remote.corba.IScriptSection read(org.omg.CORBA.portable.InputStream _input) {
    com.netscape.jsdebugging.remote.corba.IScriptSection result = new com.netscape.jsdebugging.remote.corba.IScriptSection();
    result.base = _input.read_long();
    result.extent = _input.read_long();
    return result;
  }
  public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.IScriptSection value) {
    _output.write_long(value.base);
    _output.write_long(value.extent);
  }
  public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.IScriptSection value) {
    org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
    write(output, value);
    any.read_value(output.create_input_stream(), type());
  }
  public static com.netscape.jsdebugging.remote.corba.IScriptSection extract(org.omg.CORBA.Any any) {
    if(!any.type().equal(type())) {
      throw new org.omg.CORBA.BAD_TYPECODE();
    }
    return read(any.create_input_stream());
  }
  private static org.omg.CORBA.TypeCode _type;
  public static org.omg.CORBA.TypeCode type() {
    if(_type == null) {
      org.omg.CORBA.StructMember[] members = new org.omg.CORBA.StructMember[2];
      members[0] = new org.omg.CORBA.StructMember("base", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[1] = new org.omg.CORBA.StructMember("extent", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      _type = _orb().create_struct_tc(id(), "IScriptSection", members);
    }
    return _type;
  }
  public static java.lang.String id() {
    return "IDL:IScriptSection:1.0";
  }
}
