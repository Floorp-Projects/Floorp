package com.netscape.jsdebugging.remote.corba;
final public class IJSThreadStateHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IJSThreadState value;
  public IJSThreadStateHolder() {
  }
  public IJSThreadStateHolder(com.netscape.jsdebugging.remote.corba.IJSThreadState value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IJSThreadStateHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IJSThreadStateHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IJSThreadStateHelper.type();
  }
}
