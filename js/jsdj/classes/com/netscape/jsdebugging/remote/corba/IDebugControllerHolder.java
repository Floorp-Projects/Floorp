package com.netscape.jsdebugging.remote.corba;
final public class IDebugControllerHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IDebugController value;
  public IDebugControllerHolder() {
  }
  public IDebugControllerHolder(com.netscape.jsdebugging.remote.corba.IDebugController value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IDebugControllerHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IDebugControllerHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IDebugControllerHelper.type();
  }
}
