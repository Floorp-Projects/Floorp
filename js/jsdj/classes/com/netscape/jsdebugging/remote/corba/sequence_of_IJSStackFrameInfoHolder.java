package com.netscape.jsdebugging.remote.corba;
final public class sequence_of_IJSStackFrameInfoHolder implements org.omg.CORBA.portable.Streamable {
  public com.netscape.jsdebugging.remote.corba.IJSStackFrameInfo[] value;
  public sequence_of_IJSStackFrameInfoHolder() {
  }
  public sequence_of_IJSStackFrameInfoHolder(com.netscape.jsdebugging.remote.corba.IJSStackFrameInfo[] value) {
    this.value = value;
  }
  public void _read(org.omg.CORBA.portable.InputStream input) {
    value = sequence_of_IJSStackFrameInfoHelper.read(input);
  }
  public void _write(org.omg.CORBA.portable.OutputStream output) {
    sequence_of_IJSStackFrameInfoHelper.write(output, value);
  }
  public org.omg.CORBA.TypeCode _type() {
    return sequence_of_IJSStackFrameInfoHelper.type();
  }
}
