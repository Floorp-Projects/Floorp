  package com.netscape.jsdebugging.remote.corba.TestInterfacePackage;
  final public class sequence_of_ThingHolder implements org.omg.CORBA.portable.Streamable {
    public com.netscape.jsdebugging.remote.corba.Thing[] value;
    public sequence_of_ThingHolder() {
    }
    public sequence_of_ThingHolder(com.netscape.jsdebugging.remote.corba.Thing[] value) {
      this.value = value;
    }
    public void _read(org.omg.CORBA.portable.InputStream input) {
      value = sequence_of_ThingHelper.read(input);
    }
    public void _write(org.omg.CORBA.portable.OutputStream output) {
      sequence_of_ThingHelper.write(output, value);
    }
    public org.omg.CORBA.TypeCode _type() {
      return sequence_of_ThingHelper.type();
    }
  }
