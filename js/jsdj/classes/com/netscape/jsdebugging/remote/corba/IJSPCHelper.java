package com.netscape.jsdebugging.remote.corba;
abstract public class IJSPCHelper {
  private static org.omg.CORBA.ORB _orb() {
    return org.omg.CORBA.ORB.init();
  }
  public static com.netscape.jsdebugging.remote.corba.IJSPC read(org.omg.CORBA.portable.InputStream _input) {
    com.netscape.jsdebugging.remote.corba.IJSPC result = new com.netscape.jsdebugging.remote.corba.IJSPC();
    result.script = com.netscape.jsdebugging.remote.corba.IScriptHelper.read(_input);
    result.offset = _input.read_long();
    return result;
  }
  public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.IJSPC value) {
    com.netscape.jsdebugging.remote.corba.IScriptHelper.write(_output, value.script);
    _output.write_long(value.offset);
  }
  public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.IJSPC value) {
    org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
    write(output, value);
    any.read_value(output.create_input_stream(), type());
  }
  public static com.netscape.jsdebugging.remote.corba.IJSPC extract(org.omg.CORBA.Any any) {
    if(!any.type().equal(type())) {
      throw new org.omg.CORBA.BAD_TYPECODE();
    }
    return read(any.create_input_stream());
  }
  private static org.omg.CORBA.TypeCode _type;
  public static org.omg.CORBA.TypeCode type() {
    if(_type == null) {
      org.omg.CORBA.StructMember[] members = new org.omg.CORBA.StructMember[2];
      members[0] = new org.omg.CORBA.StructMember("script", com.netscape.jsdebugging.remote.corba.IScriptHelper.type(), null);
      members[1] = new org.omg.CORBA.StructMember("offset", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      _type = _orb().create_struct_tc(id(), "IJSPC", members);
    }
    return _type;
  }
  public static java.lang.String id() {
    return "IDL:IJSPC:1.0";
  }
}
