package com.netscape.jsdebugging.remote.corba;
abstract public class sequence_of_IScriptSectionHelper {
  private static org.omg.CORBA.ORB _orb() {
    return org.omg.CORBA.ORB.init();
  }
  public static com.netscape.jsdebugging.remote.corba.IScriptSection[] read(org.omg.CORBA.portable.InputStream _input) {
    com.netscape.jsdebugging.remote.corba.IScriptSection[] result;
    {
      int _length3 = _input.read_long();
      result = new com.netscape.jsdebugging.remote.corba.IScriptSection[_length3];
      for(int _i3 = 0; _i3 < _length3; _i3++) {
        result[_i3] = com.netscape.jsdebugging.remote.corba.IScriptSectionHelper.read(_input);
      }
    }
    return result;
  }
  public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.IScriptSection[] value) {
    _output.write_long(value.length);
    for(int _i2 = 0; _i2 < value.length; _i2++) {
      com.netscape.jsdebugging.remote.corba.IScriptSectionHelper.write(_output, value[_i2]);
    }
  }
  public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.IScriptSection[] value) {
    org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
    write(output, value);
    any.read_value(output.create_input_stream(), type());
  }
  public static com.netscape.jsdebugging.remote.corba.IScriptSection[] extract(org.omg.CORBA.Any any) {
    if(!any.type().equal(type())) {
      throw new org.omg.CORBA.BAD_TYPECODE();
    }
    return read(any.create_input_stream());
  }
  private static org.omg.CORBA.TypeCode _type;
  public static org.omg.CORBA.TypeCode type() {
    if(_type == null) {
      org.omg.CORBA.TypeCode original_type = _orb().create_sequence_tc(0, com.netscape.jsdebugging.remote.corba.IScriptSectionHelper.type());
      _type = _orb().create_alias_tc(id(), "sequence_of_IScriptSection", original_type);
    }
    return _type;
  }
  public static java.lang.String id() {
    return "IDL:sequence_of_IScriptSection:1.0";
  }
}
