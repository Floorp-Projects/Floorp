  package com.netscape.jsdebugging.remote.corba.TestInterfacePackage;
  abstract public class sequence_of_ThingHelper {
    private static org.omg.CORBA.ORB _orb() {
      return org.omg.CORBA.ORB.init();
    }
    public static com.netscape.jsdebugging.remote.corba.Thing[] read(org.omg.CORBA.portable.InputStream _input) {
      com.netscape.jsdebugging.remote.corba.Thing[] result;
      {
        int _length4 = _input.read_long();
        result = new com.netscape.jsdebugging.remote.corba.Thing[_length4];
        for(int _i4 = 0; _i4 < _length4; _i4++) {
          result[_i4] = com.netscape.jsdebugging.remote.corba.ThingHelper.read(_input);
        }
      }
      return result;
    }
    public static void write(org.omg.CORBA.portable.OutputStream _output, com.netscape.jsdebugging.remote.corba.Thing[] value) {
      _output.write_long(value.length);
      for(int _i3 = 0; _i3 < value.length; _i3++) {
        com.netscape.jsdebugging.remote.corba.ThingHelper.write(_output, value[_i3]);
      }
    }
    public static void insert(org.omg.CORBA.Any any, com.netscape.jsdebugging.remote.corba.Thing[] value) {
      org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
      write(output, value);
      any.read_value(output.create_input_stream(), type());
    }
    public static com.netscape.jsdebugging.remote.corba.Thing[] extract(org.omg.CORBA.Any any) {
      if(!any.type().equal(type())) {
        throw new org.omg.CORBA.BAD_TYPECODE();
      }
      return read(any.create_input_stream());
    }
    private static org.omg.CORBA.TypeCode _type;
    public static org.omg.CORBA.TypeCode type() {
      if(_type == null) {
        org.omg.CORBA.TypeCode original_type = _orb().create_sequence_tc(0, com.netscape.jsdebugging.remote.corba.ThingHelper.type());
        _type = _orb().create_alias_tc(id(), "sequence_of_Thing", original_type);
      }
      return _type;
    }
    public static java.lang.String id() {
      return "IDL:TestInterface/sequence_of_Thing:1.0";
    }
  }
