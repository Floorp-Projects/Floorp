package com.netscape.jsdebugging.remote.corba;
final public class IJSStackFrameInfoHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IJSStackFrameInfo value;
  public IJSStackFrameInfoHolder() {
  }
  public IJSStackFrameInfoHolder(com.netscape.jsdebugging.remote.corba.IJSStackFrameInfo value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = IJSStackFrameInfoHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    IJSStackFrameInfoHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return IJSStackFrameInfoHelper.type();
  }
}
