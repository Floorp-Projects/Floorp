  package com.netscape.jsdebugging.remote.corba.ISourceTextProviderPackage;
  abstract public class sequence_of_stringHelper {
    private static org.omg.CORBA.ORB _orb() {
      return org.omg.CORBA.ORB.init();
    }
    public static java.lang.String[] read(org.omg.CORBA.portable.InputStream _input) {
      java.lang.String[] result;
      {
        int _length4 = _input.read_long();
        result = new java.lang.String[_length4];
        for(int _i4 = 0; _i4 < _length4; _i4++) {
          result[_i4] = _input.read_string();
        }
      }
      return result;
    }
    public static void write(org.omg.CORBA.portable.OutputStream _output, java.lang.String[] value) {
      _output.write_long(value.length);
      for(int _i3 = 0; _i3 < value.length; _i3++) {
        _output.write_string(value[_i3]);
      }
    }
    public static void insert(org.omg.CORBA.Any any, java.lang.String[] value) {
      org.omg.CORBA.portable.OutputStream output = any.create_output_stream();
      write(output, value);
      any.read_value(output.create_input_stream(), type());
    }
    public static java.lang.String[] extract(org.omg.CORBA.Any any) {
      if(!any.type().equal(type())) {
        throw new org.omg.CORBA.BAD_TYPECODE();
      }
      return read(any.create_input_stream());
    }
    private static org.omg.CORBA.TypeCode _type;
    public static org.omg.CORBA.TypeCode type() {
      if(_type == null) {
        org.omg.CORBA.TypeCode original_type = _orb().create_sequence_tc(0, _orb().get_primitive_tc(org.omg.CORBA.TCKind.tk_string));
        _type = _orb().create_alias_tc(id(), "sequence_of_string", original_type);
      }
      return _type;
    }
    public static java.lang.String id() {
      return "IDL:ISourceTextProvider/sequence_of_string:1.0";
    }
  }
