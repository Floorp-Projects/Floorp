  package com.netscape.jsdebugging.remote.corba.ISourceTextProviderPackage;
  final public class sequence_of_stringHolder implements org.omg.CORBA.portable.Streamable {
    public java.lang.String[] value;
    public sequence_of_stringHolder() {
    }
    public sequence_of_stringHolder(java.lang.String[] value) {
      this.value = value;
    }
    public void _read(org.omg.CORBA.portable.InputStream input) {
      value = sequence_of_stringHelper.read(input);
    }
    public void _write(org.omg.CORBA.portable.OutputStream output) {
      sequence_of_stringHelper.write(output, value);
    }
    public org.omg.CORBA.TypeCode _type() {
      return sequence_of_stringHelper.type();
    }
  }
