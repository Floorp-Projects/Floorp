package com.netscape.jsdebugging.remote.corba;
final public class StringRecieverHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.StringReciever value;
  public StringRecieverHolder() {
  }
  public StringRecieverHolder(com.netscape.jsdebugging.remote.corba.StringReciever value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = StringRecieverHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    StringRecieverHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return StringRecieverHelper.type();
  }
}
