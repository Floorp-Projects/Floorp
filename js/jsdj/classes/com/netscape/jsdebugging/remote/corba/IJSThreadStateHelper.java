package com.netscape.jsdebugging.remote.corba;
abstract public class IJSThreadStateHelper {
  private static org.omg.CORBA.ORB _orb() {
    return org.omg.CORBA.ORB.init();
  }
  public static com.netscape.jsdebugging.remote.corba.IJSThreadState read(org.omg.CORBA.portable.InputStream _input) {
    com.netscape.jsdebugging.remote.corba.IJSThreadState result = new com.netscape.jsdebugging.remote.corba.IJSThreadState();
    result.stack = com.netscape.jsdebugging.remote.corba.sequence_of_IJSStackFrameInfoHelper.read(_input);
    result.continueState = _input.read_long();
    result.returnValue = _input.read_string();
    result.status = _input.read_long();
    result.jsdthreadstate = _input.read_long();
    result.id = _input.read_long();
    return result;
  }
  public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.IJSThreadState value) {
    com.netscape.jsdebugging.remote.corba.sequence_of_IJSStackFrameInfoHelper.write(_output, value.stack);
    _output.write_long(value.continueState);
    _output.write_string(value.returnValue);
    _output.write_long(value.status);
    _output.write_long(value.jsdthreadstate);
    _output.write_long(value.id);
  }
  public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.IJSThreadState value) {
    org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
    write(output, value);
    any.read_value(output.create_input_stream(), type());
  }
  public static com.netscape.jsdebugging.remote.corba.IJSThreadState extract(org.omg.CORBA.Any any) {
    if(!any.type().equal(type())) {
      throw new org.omg.CORBA.BAD_TYPECODE();
    }
    return read(any.create_input_stream());
  }
  private static org.omg.CORBA.TypeCode _type;
  public static org.omg.CORBA.TypeCode type() {
    if(_type == null) {
      org.omg.CORBA.StructMember[] members = new org.omg.CORBA.StructMember[6];
      members[0] = new org.omg.CORBA.StructMember("stack", _orb().create_sequence_tc(0, com.netscape.jsdebugging.remote.corba.IJSStackFrameInfoHelper.type()), null);
      members[1] = new org.omg.CORBA.StructMember("continueState", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[2] = new org.omg.CORBA.StructMember("returnValue", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_string), null);
      members[3] = new org.omg.CORBA.StructMember("status", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[4] = new org.omg.CORBA.StructMember("jsdthreadstate", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      members[5] = new org.omg.CORBA.StructMember("id", _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_long), null);
      _type = _orb().create_struct_tc(id(), "IJSThreadState", members);
    }
    return _type;
  }
  public static java.lang.String id() {
    return "IDL:IJSThreadState:1.0";
  }
}
