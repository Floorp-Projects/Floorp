package com.netscape.jsdebugging.remote.corba;
abstract public class IJSSourceLocationHelper {
  private static org.omg.CORBA.ORB _orb() {
    return org.omg.CORBA.ORB.init();
  }
  public static com.netscape.jsdebugging.remote.corba.IJSSourceLocation read(org.omg.CORBA.portable.InputStream _input) {
    com.netscape.jsdebugging.remote.corba.IJSSourceLocation result = new com.netscape.jsdebugging.remote.corba.IJSSourceLocation();
    result.line = _input.read_long();
    result.pc = com.netscape.jsdebugging.remote.corba.IJSPCHelper.read(_input);
    return result;
  }
  public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.IJSSourceLocation value) {
    _output.write_long(value.line);
    com.netscape.jsdebugging.remote.corba.IJSPCHelper.write(_output, value.pc);
  }
  public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.IJSSourceLocation value) {
    org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
    write(output, value);
    any.read_value(output.create_input_stream(), type());
  }
  public static com.netscape.jsdebugging.remote.corba.IJSSourceLocation extract(org.omg.CORBA.Any any) {
    if(!any.type().equal(type())) {
      throw new org.omg.CORBA.BAD_TYPECODE();
    }
    return read(any.create_input_stream());
  }
  private static org.omg.CORBA.TypeCode _type;
  public static org.omg.CORBA.TypeCode type() {
    if(_type == null) {
      org.omg.CORBA.StructMember[] members = new org.omg.CORBA.StructMember[2];
      members[0] = new org.omg.CORBA.StructMember("line", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[1] = new org.omg.CORBA.StructMember("pc", com.netscape.jsdebugging.remote.corba.IJSPCHelper.type(), null);
      _type = _orb().create_struct_tc(id(), "IJSSourceLocation", members);
    }
    return _type;
  }
  public static java.lang.String id() {
    return "IDL:IJSSourceLocation:1.0";
  }
}
