package com.netscape.jsdebugging.remote.corba;
final public class ThingHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.Thing value;
  public ThingHolder() {
  }
  public ThingHolder(com.netscape.jsdebugging.remote.corba.Thing value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = ThingHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    ThingHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return ThingHelper.type();
  }
}
