package com.netscape.jsdebugging.remote.corba;
abstract public class IScriptHelper {
  private static org.omg.CORBA.ORB _orb() {
    return org.omg.CORBA.ORB.init();
  }
  public static com.netscape.jsdebugging.remote.corba.IScript read(org.omg.CORBA.portable.InputStream _input) {
    com.netscape.jsdebugging.remote.corba.IScript result = new com.netscape.jsdebugging.remote.corba.IScript();
    result.url = _input.read_string();
    result.funname = _input.read_string();
    result.base = _input.read_long();
    result.extent = _input.read_long();
    result.jsdscript = _input.read_long();
    result.sections = com.netscape.jsdebugging.remote.corba.sequence_of_IScriptSectionHelper.read(_input);
    return result;
  }
  public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.IScript value) {
    _output.write_string(value.url);
    _output.write_string(value.funname);
    _output.write_long(value.base);
    _output.write_long(value.extent);
    _output.write_long(value.jsdscript);
    com.netscape.jsdebugging.remote.corba.sequence_of_IScriptSectionHelper.write(_output, value.sections);
  }
  public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.IScript value) {
    org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
    write(output, value);
    any.read_value(output.create_input_stream(), type());
  }
  public static com.netscape.jsdebugging.remote.corba.IScript extract(org.omg.CORBA.Any any) {
    if(!any.type().equal(type())) {
      throw new org.omg.CORBA.BAD_TYPECODE();
    }
    return read(any.create_input_stream());
  }
  private static org.omg.CORBA.TypeCode _type;
  public static org.omg.CORBA.TypeCode type() {
    if(_type == null) {
      org.omg.CORBA.StructMember[] members = new org.omg.CORBA.StructMember[6];
      members[0] = new org.omg.CORBA.StructMember("url", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_string), null);
      members[1] = new org.omg.CORBA.StructMember("funname", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_string), null);
      members[2] = new org.omg.CORBA.StructMember("base", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[3] = new org.omg.CORBA.StructMember("extent", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[4] = new org.omg.CORBA.StructMember("jsdscript", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[5] = new org.omg.CORBA.StructMember("sections", _orb().create_sequence_tc(0, com.netscape.jsdebugging.remote.corba.IScriptSectionHelper.type()), null);
      _type = _orb().create_struct_tc(id(), "IScript", members);
    }
    return _type;
  }
  public static java.lang.String id() {
    return "IDL:IScript:1.0";
  }
}
