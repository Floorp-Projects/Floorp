package com.netscape.jsdebugging.remote.corba;
abstract public class IExecResultHelper {
  private static org.omg.CORBA.ORB _orb() {
    return org.omg.CORBA.ORB.init();
  }
  public static com.netscape.jsdebugging.remote.corba.IExecResult read(org.omg.CORBA.portable.InputStream _input) {
    com.netscape.jsdebugging.remote.corba.IExecResult result = new com.netscape.jsdebugging.remote.corba.IExecResult();
    result.result = _input.read_string();
    result.errorOccured = _input.read_boolean();
    result.errorMessage = _input.read_string();
    result.errorFilename = _input.read_string();
    result.errorLineNumber = _input.read_long();
    result.errorLineBuffer = _input.read_string();
    result.errorTokenOffset = _input.read_long();
    return result;
  }
  public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.IExecResult value) {
    _output.write_string(value.result);
    _output.write_boolean(value.errorOccured);
    _output.write_string(value.errorMessage);
    _output.write_string(value.errorFilename);
    _output.write_long(value.errorLineNumber);
    _output.write_string(value.errorLineBuffer);
    _output.write_long(value.errorTokenOffset);
  }
  public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.IExecResult value) {
    org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
    write(output, value);
    any.read_value(output.create_input_stream(), type());
  }
  public static com.netscape.jsdebugging.remote.corba.IExecResult extract(org.omg.CORBA.Any any) {
    if(!any.type().equal(type())) {
      throw new org.omg.CORBA.BAD_TYPECODE();
    }
    return read(any.create_input_stream());
  }
  private static org.omg.CORBA.TypeCode _type;
  public static org.omg.CORBA.TypeCode type() {
    if(_type == null) {
      org.omg.CORBA.StructMember[] members = new org.omg.CORBA.StructMember[7];
      members[0] = new org.omg.CORBA.StructMember("result", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_string), null);
      members[1] = new org.omg.CORBA.StructMember("errorOccured", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_boolean), null);
      members[2] = new org.omg.CORBA.StructMember("errorMessage", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_string), null);
      members[3] = new org.omg.CORBA.StructMember("errorFilename", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_string), null);
      members[4] = new org.omg.CORBA.StructMember("errorLineNumber", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[5] = new org.omg.CORBA.StructMember("errorLineBuffer", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_string), null);
      members[6] = new org.omg.CORBA.StructMember("errorTokenOffset", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      _type = _orb().create_struct_tc(id(), "IExecResult", members);
    }
    return _type;
  }
  public static java.lang.String id() {
    return "IDL:IExecResult:1.0";
  }
}
